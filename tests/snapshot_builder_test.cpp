#include "ghinfo/snapshot_builder.hpp"

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

void register_repository(httplib::Server& server, const std::string& full_name,
                         bool healthy = true) {
    const auto repository_body = std::string{"{\"id\":"} +
                                 (full_name == "a/repo" ? "1001" : "1002") + ",\"full_name\":\"" +
                                 full_name +
                                 "\",\"private\":false,\"default_branch\":\"main\","
                                 "\"html_url\":\"https://github.com/" +
                                 full_name + "\",\"updated_at\":\"2026-08-26T13:00:00Z\"}";
    const auto issues = read_fixture("tests/fixtures/github/issues.json");
    const auto pull_requests = read_fixture("tests/fixtures/github/pulls.json");
    const auto runs = read_fixture("tests/fixtures/github/runs.json");
    const auto jobs = read_fixture("tests/fixtures/github/jobs.json");
    const auto add_rate_headers = [](httplib::Response& response) {
        response.set_header("X-RateLimit-Limit", "5000");
        response.set_header("X-RateLimit-Remaining", "4999");
        response.set_header("X-RateLimit-Reset", "0");
    };

    server.Get("/repos/" + full_name, [repository_body, add_rate_headers, healthy](
                                          const httplib::Request&, httplib::Response& response) {
        if (!healthy) {
            response.status = 503;
            return;
        }
        add_rate_headers(response);
        response.set_content(repository_body, "application/json");
    });
    server.Get(
        "/repos/" + full_name + "/issues",
        [issues, add_rate_headers, healthy](const httplib::Request&, httplib::Response& response) {
            if (!healthy) {
                response.status = 503;
                return;
            }
            add_rate_headers(response);
            response.set_content(issues, "application/json");
        });
    server.Get("/repos/" + full_name + "/pulls",
               [pull_requests, add_rate_headers, healthy](const httplib::Request&,
                                                          httplib::Response& response) {
                   if (!healthy) {
                       response.status = 503;
                       return;
                   }
                   add_rate_headers(response);
                   response.set_content(pull_requests, "application/json");
               });
    server.Get(
        "/repos/" + full_name + "/actions/runs",
        [runs, add_rate_headers, healthy](const httplib::Request&, httplib::Response& response) {
            if (!healthy) {
                response.status = 503;
                return;
            }
            add_rate_headers(response);
            response.set_content(runs, "application/json");
        });
    server.Get(
        "/repos/" + full_name + "/actions/runs/3001/jobs",
        [jobs, add_rate_headers, healthy](const httplib::Request&, httplib::Response& response) {
            if (!healthy) {
                response.status = 503;
                return;
            }
            add_rate_headers(response);
            response.set_content(jobs, "application/json");
        });
}

TEST(SnapshotBuilderTest, BuildsCompleteDeterministicSnapshot) {
    LocalHttpServer server;
    register_repository(server.server(), "z/repo");
    register_repository(server.server(), "a/repo");

    ghinfo::Config config;
    config.repositories = {
        ghinfo::parse_repository_ref("z/repo"),
        ghinfo::parse_repository_ref("a/repo"),
    };
    config.run_history = 20;
    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto snapshot = ghinfo::build_snapshot(config, client, 7, "2026-08-26T18:00:00Z");

    EXPECT_EQ(snapshot.generation, 7U);
    EXPECT_EQ(snapshot.generated_at, "2026-08-26T18:00:00Z");
    EXPECT_EQ(snapshot.last_successful_poll, "2026-08-26T18:00:00Z");
    ASSERT_EQ(snapshot.repositories.size(), 2U);
    EXPECT_EQ(snapshot.repositories[0].full_name, "a/repo");
    EXPECT_EQ(snapshot.repositories[1].full_name, "z/repo");
    EXPECT_EQ(snapshot.issues.size(), 2U);
    EXPECT_EQ(snapshot.issues[0].repository, "a/repo");
    EXPECT_EQ(snapshot.pull_requests.size(), 2U);
    EXPECT_EQ(snapshot.pull_requests[0].repository, "a/repo");
    EXPECT_EQ(snapshot.workflow_runs.size(), 2U);
    EXPECT_EQ(snapshot.jobs.size(), 2U);
    EXPECT_EQ(snapshot.jobs[0].repository, "a/repo");
    ASSERT_TRUE(snapshot.rate_limit.has_value());
    EXPECT_EQ(snapshot.rate_limit->limit, 5000U);
    EXPECT_EQ(snapshot.rate_limit->remaining, 4999U);
    ASSERT_TRUE(snapshot.rate_limit->reset_at.has_value());
    EXPECT_EQ(snapshot.rate_limit->reset_at.value(), "1970-01-01T00:00:00Z");
}

TEST(SnapshotBuilderTest, DiscoversRepositoriesWhenConfiguredForAutomaticSelection) {
    LocalHttpServer server;
    server.server().Get("/user/repos", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(R"([{"full_name":"a/repo"}])", "application/json");
    });
    register_repository(server.server(), "a/repo");

    ghinfo::Config config;
    config.repository_selection = ghinfo::RepositorySelection::discover_all;
    config.run_history = 20;
    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto snapshot = ghinfo::build_snapshot(config, client, 9, "2026-08-26T19:00:00Z");

    ASSERT_EQ(snapshot.repositories.size(), 1U);
    EXPECT_EQ(snapshot.repositories.front().full_name, "a/repo");
}

TEST(SnapshotBuilderTest, FormatsCurrentTimeAsUtcIso8601) {
    const auto timestamp = ghinfo::utc_now_iso8601();

    ASSERT_EQ(timestamp.size(), 20U);
    EXPECT_EQ(timestamp.back(), 'Z');
    EXPECT_EQ(timestamp[4], '-');
    EXPECT_EQ(timestamp[10], 'T');
    EXPECT_EQ(timestamp[13], ':');
    EXPECT_EQ(timestamp[16], ':');
}

TEST(SnapshotBuilderTest, RejectsPartialRefreshWhenAnyRepositoryFails) {
    LocalHttpServer server;
    register_repository(server.server(), "a/repo");
    register_repository(server.server(), "b/repo", false);

    ghinfo::Config config;
    config.repositories = {
        ghinfo::parse_repository_ref("a/repo"),
        ghinfo::parse_repository_ref("b/repo"),
    };
    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    EXPECT_THROW((void)ghinfo::build_snapshot(config, client, 8, "2026-08-26T19:00:00Z"),
                 ghinfo::GitHubRequestError);
}

} // namespace
