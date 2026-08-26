#include "ghinfo/poller.hpp"

#include <httplib.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::string read_fixture(const std::string& relative_path) {
    std::ifstream file{std::string{GHINFO_SOURCE_DIR} + "/" + relative_path};
    if (!file) {
        throw std::runtime_error("failed to read test fixture");
    }
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

class LocalHttpServer {
  public:
    LocalHttpServer() {
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) {
            throw std::runtime_error("failed to bind local test server");
        }
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        for (int attempt = 0; attempt < 100 && !server_.is_running(); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        if (!server_.is_running()) {
            server_.stop();
            thread_.join();
            throw std::runtime_error("local test server did not start");
        }
    }

    ~LocalHttpServer() {
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    httplib::Server& server() {
        return server_;
    }

  private:
    httplib::Server server_;
    std::thread thread_;
    int port_{};
};

void register_repository(httplib::Server& server) {
    const auto issues = read_fixture("tests/fixtures/github/issues.json");
    const auto pull_requests = read_fixture("tests/fixtures/github/pulls.json");
    const auto runs = read_fixture("tests/fixtures/github/runs.json");
    const auto jobs = read_fixture("tests/fixtures/github/jobs.json");

    server.Get("/repos/owner/repo", [](const httplib::Request&, httplib::Response& response) {
        response.set_header("X-RateLimit-Limit", "5000");
        response.set_header("X-RateLimit-Remaining", "4999");
        response.set_header("X-RateLimit-Reset", "0");
        response.set_content(
            "{\"id\":1001,\"full_name\":\"owner/repo\",\"private\":false,"
            "\"default_branch\":\"main\",\"html_url\":\"https://github.com/owner/repo\","
            "\"updated_at\":\"2026-08-26T13:00:00Z\"}",
            "application/json");
    });
    server.Get("/repos/owner/repo/issues",
               [issues](const httplib::Request&, httplib::Response& response) {
                   response.set_content(issues, "application/json");
               });
    server.Get("/repos/owner/repo/pulls",
               [pull_requests](const httplib::Request&, httplib::Response& response) {
                   response.set_content(pull_requests, "application/json");
               });
    server.Get("/repos/owner/repo/actions/runs",
               [runs](const httplib::Request&, httplib::Response& response) {
                   response.set_content(runs, "application/json");
               });
    server.Get("/repos/owner/repo/actions/runs/3001/jobs",
               [jobs](const httplib::Request&, httplib::Response& response) {
                   response.set_content(jobs, "application/json");
               });
}

TEST(PollerTest, PublishesFirstSnapshotAndStopsPromptly) {
    LocalHttpServer server;
    register_repository(server.server());

    ghinfo::Config config;
    config.repositories = {ghinfo::parse_repository_ref("owner/repo")};
    config.poll_interval_seconds = 3600;
    config.run_history = 20;
    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };
    ghinfo::SnapshotStore store;
    ghinfo::Poller poller{config, client, store};

    std::jthread worker{[&poller](std::stop_token stop_token) { poller.run(stop_token); }};
    for (int attempt = 0; attempt < 500 && !store.ready(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    ASSERT_TRUE(store.ready());
    const auto first = store.get();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->generation, 1U);
    EXPECT_EQ(first->repositories.size(), 1U);
    EXPECT_EQ(first->issues.size(), 1U);
    EXPECT_EQ(first->pull_requests.size(), 1U);
    EXPECT_EQ(first->workflow_runs.size(), 1U);
    EXPECT_EQ(first->jobs.size(), 1U);

    const auto stop_started = std::chrono::steady_clock::now();
    worker.request_stop();
    worker.join();
    const auto stop_duration = std::chrono::steady_clock::now() - stop_started;
    EXPECT_LT(stop_duration, std::chrono::milliseconds{500});

    std::jthread second_worker{[&poller](std::stop_token stop_token) { poller.run(stop_token); }};
    for (int attempt = 0; attempt < 500 && store.get()->generation < 2; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    const auto second = store.get();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->generation, 2U);
    second_worker.request_stop();
    second_worker.join();
}

} // namespace
