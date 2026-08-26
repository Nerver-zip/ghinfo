#include "ghinfo/server.hpp"

#include <httplib.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

std::shared_ptr<ghinfo::Snapshot> sample_snapshot() {
    auto snapshot = std::make_shared<ghinfo::Snapshot>();
    snapshot->generation = 7;
    snapshot->generated_at = "2026-08-26T20:45:31Z";
    snapshot->last_successful_poll = snapshot->generated_at;
    snapshot->repositories = {
        ghinfo::Repository{1001, "owner/repo", false, "main", "https://github.com/owner/repo",
                           "2026-08-26T13:00:00Z"},
        ghinfo::Repository{1002, "other/repo", true, "trunk", "https://github.com/other/repo",
                           "2026-08-26T14:00:00Z"},
    };
    snapshot->issues = {
        ghinfo::Issue{1003,
                      42,
                      "owner/repo",
                      "Example issue",
                      "octocat",
                      {"bug"},
                      "2026-08-26T12:00:00Z",
                      "2026-08-26T13:00:00Z",
                      "https://github.com/owner/repo/issues/42"},
        ghinfo::Issue{1004,
                      7,
                      "other/repo",
                      "Other issue",
                      "hubot",
                      {},
                      "2026-08-26T11:00:00Z",
                      "2026-08-26T12:00:00Z",
                      "https://github.com/other/repo/issues/7"},
    };
    snapshot->pull_requests = {
        ghinfo::PullRequest{2001, 17, "owner/repo", "Example pull request", "octocat", true,
                            "feature/example", "main", "2026-08-26T12:00:00Z",
                            "2026-08-26T13:00:00Z", "https://github.com/owner/repo/pull/17"},
    };
    snapshot->workflow_runs = {
        ghinfo::WorkflowRun{3001, "owner/repo", "CI", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "0123456789abcdef", "push",
                            "2026-08-26T12:00:00Z", "2026-08-26T12:03:00Z",
                            "https://github.com/owner/repo/actions/runs/3001"},
        ghinfo::WorkflowRun{3002, "other/repo", "Checks", ghinfo::RunStatus::queued, std::nullopt,
                            "trunk", "fedcba9876543210", "pull_request", "2026-08-26T12:10:00Z",
                            "2026-08-26T12:10:00Z",
                            "https://github.com/other/repo/actions/runs/3002"},
    };
    snapshot->jobs = {
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "gcc-debug", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-08-26T12:00:10Z"},
                            std::string{"2026-08-26T12:02:55Z"},
                            "https://github.com/owner/repo/actions/runs/3001/job/4001"},
    };
    snapshot->rate_limit = ghinfo::RateLimit{5000, 4999, "2026-08-26T21:00:00Z"};
    return snapshot;
}

std::string read_fixture(const std::string& relative_path) {
    std::ifstream file{std::string{GHINFO_SOURCE_DIR} + "/" + relative_path};
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

int available_port() {
    const auto socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("failed to create test socket");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        close(socket_fd);
        throw std::runtime_error("failed to reserve test port");
    }
    socklen_t length = static_cast<socklen_t>(sizeof(address));
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        close(socket_fd);
        throw std::runtime_error("failed to inspect test port");
    }
    const auto port = ntohs(address.sin_port);
    close(socket_fd);
    return static_cast<int>(port);
}

struct ApiListenerGuard {
    ghinfo::ApiServer& api;
    std::thread& listener;

    ~ApiListenerGuard() {
        api.stop();
        if (listener.joinable()) {
            listener.join();
        }
    }
};

TEST(ApiTest, HealthPayloadIsAlwaysOk) {
    const auto response = ghinfo::make_health_response();

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("status"), "ok");
}

TEST(ApiTest, ReadyPayloadReturns503BeforeSnapshot) {
    const auto response = ghinfo::make_readiness_response(false);

    EXPECT_EQ(response.status, 503);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_FALSE(body.at("ready").get<bool>());
}

TEST(ApiTest, ReadyPayloadReturns200AfterSnapshot) {
    const auto response = ghinfo::make_readiness_response(true);

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_TRUE(body.at("ready").get<bool>());
}

TEST(ApiTest, MetaPayloadIsVersioned) {
    const auto response = ghinfo::make_meta_response(false);

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("schemaVersion"), 1);
    EXPECT_EQ(body.at("service"), "ghinfo");
    EXPECT_FALSE(body.at("snapshotAvailable").get<bool>());
}

TEST(ApiTest, SummaryMatchesGoldenContract) {
    ghinfo::SnapshotStore store;
    store.publish(sample_snapshot());
    store.record_poll_success("2026-08-26T20:45:31Z");

    const auto response = ghinfo::make_summary_response(store);
    ASSERT_EQ(response.status, 200);
    const auto actual = nlohmann::json::parse(response.body);
    const auto expected = nlohmann::json::parse(read_fixture("tests/golden/summary.json"));
    EXPECT_EQ(actual, expected);
}

TEST(ApiTest, ResourcePayloadsNormalizeAndFilterSnapshotData) {
    const auto snapshot = sample_snapshot();

    const auto repositories =
        nlohmann::json::parse(ghinfo::make_repositories_response(*snapshot).body);
    EXPECT_EQ(repositories.at("repositories").size(), 2U);
    EXPECT_EQ(repositories.at("repositories").at(0).at("fullName"), "owner/repo");
    EXPECT_EQ(repositories.at("repositories").at(0).at("defaultBranch"), "main");

    const auto repository =
        nlohmann::json::parse(ghinfo::make_repository_response(*snapshot, "owner/repo").body);
    EXPECT_EQ(repository.at("repository").at("fullName"), "owner/repo");
    EXPECT_EQ(repository.at("issues").size(), 1U);
    EXPECT_EQ(repository.at("pullRequests").size(), 1U);
    EXPECT_EQ(repository.at("workflowRuns").size(), 1U);
    EXPECT_EQ(repository.at("jobs").size(), 1U);

    const auto issues = nlohmann::json::parse(
        ghinfo::make_issues_response(*snapshot, std::string{"other/repo"}).body);
    EXPECT_EQ(issues.at("issues").size(), 1U);
    EXPECT_EQ(issues.at("issues").at(0).at("repository"), "other/repo");

    const auto runs =
        nlohmann::json::parse(ghinfo::make_workflow_runs_response(
                                  *snapshot, std::nullopt, std::string{"queued"}, std::nullopt)
                                  .body);
    ASSERT_EQ(runs.at("workflowRuns").size(), 1U);
    EXPECT_EQ(runs.at("workflowRuns").at(0).at("status"), "queued");
    EXPECT_TRUE(runs.at("workflowRuns").at(0).at("conclusion").is_null());

    const auto failed_runs =
        nlohmann::json::parse(ghinfo::make_workflow_runs_response(
                                  *snapshot, std::nullopt, std::nullopt, std::string{"failure"})
                                  .body);
    ASSERT_EQ(failed_runs.at("workflowRuns").size(), 1U);
    EXPECT_EQ(failed_runs.at("workflowRuns").at(0).at("conclusion"), "failure");
}

TEST(ApiTest, DataEndpointsReportUnavailableBeforeFirstSnapshot) {
    const auto response = ghinfo::make_snapshot_unavailable_response();

    EXPECT_EQ(response.status, 503);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("schemaVersion"), 1);
    EXPECT_EQ(body.at("error"), "snapshot_unavailable");
}

TEST(ApiTest, ActivityGroupsOnlyObjectiveSnapshotState) {
    ghinfo::SnapshotStore store;
    auto snapshot = sample_snapshot();
    snapshot->jobs.push_back(ghinfo::WorkflowJob{
        4002, 3002, "other/repo", "pending-check", ghinfo::RunStatus::in_progress, std::nullopt,
        std::nullopt, std::nullopt, "https://github.com/other/repo/job/4002"});
    store.publish(std::move(snapshot));
    store.record_poll_success("2026-08-26T20:45:31Z");

    const auto response = ghinfo::make_activity_response(store);
    ASSERT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    ASSERT_EQ(body.at("activity").at("runningJobs").size(), 1U);
    EXPECT_EQ(body.at("activity").at("runningJobs").at(0).at("name"), "pending-check");
    ASSERT_EQ(body.at("activity").at("failedRuns").size(), 1U);
    EXPECT_EQ(body.at("activity").at("failedRuns").at(0).at("conclusion"), "failure");
    EXPECT_EQ(body.at("activity").at("pullRequests").size(), 1U);
    EXPECT_EQ(body.at("activity").at("issues").size(), 2U);
    EXPECT_FALSE(body.at("activity").contains("priority"));
    EXPECT_FALSE(body.at("activity").contains("score"));
}

TEST(ApiTest, MetaIncludesSafePollAndRateLimitState) {
    ghinfo::SnapshotStore store;
    store.publish(sample_snapshot());
    store.record_poll_success("2026-08-26T20:45:31Z");

    const auto body = nlohmann::json::parse(ghinfo::make_meta_response(store).body);
    EXPECT_EQ(body.at("generation"), 7U);
    EXPECT_FALSE(body.at("poll").at("stale").get<bool>());
    EXPECT_EQ(body.at("poll").at("consecutiveFailures"), 0U);
    EXPECT_EQ(body.at("rateLimit").at("remaining"), 4999U);
}

TEST(ApiTest, ServesAllPlannedRoutesAndFiltersOverHttp) {
    ghinfo::SnapshotStore store;
    store.publish(sample_snapshot());
    store.record_poll_success("2026-08-26T20:45:31Z");

    ghinfo::Config config;
    config.bind_address = "127.0.0.1";
    config.port = static_cast<std::uint16_t>(available_port());
    ghinfo::ApiServer api{config, store};
    std::atomic<bool> listen_finished{false};
    std::atomic<bool> listen_succeeded{false};
    std::thread listener{[&] {
        listen_succeeded.store(api.listen(), std::memory_order_release);
        listen_finished.store(true, std::memory_order_release);
    }};
    ApiListenerGuard listener_guard{api, listener};

    httplib::Client client{"127.0.0.1", static_cast<int>(config.port)};
    client.set_connection_timeout(0, 200000);
    httplib::Result health;
    for (int attempt = 0; attempt < 100 && !health; ++attempt) {
        health = client.Get("/healthz");
        if (!health) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }
    if (!health) {
        api.stop();
        listener.join();
        FAIL() << "HTTP server did not become reachable";
    }
    EXPECT_FALSE(listen_finished.load(std::memory_order_acquire));
    ASSERT_EQ(health->status, 200);
    EXPECT_EQ(nlohmann::json::parse(health->body).at("status"), "ok");
    EXPECT_EQ(health->get_header_value("Content-Type"), "application/json");

    const auto readiness = client.Get("/readyz");
    ASSERT_TRUE(readiness != nullptr);
    EXPECT_EQ(readiness->status, 200);
    EXPECT_TRUE(nlohmann::json::parse(readiness->body).at("ready").get<bool>());

    const auto meta = client.Get("/v1/meta");
    ASSERT_TRUE(meta != nullptr);
    EXPECT_EQ(meta->status, 200);
    EXPECT_EQ(nlohmann::json::parse(meta->body).at("service"), "ghinfo");

    const auto summary = client.Get("/v1/summary");
    ASSERT_TRUE(summary != nullptr);
    EXPECT_EQ(summary->status, 200);
    EXPECT_EQ(nlohmann::json::parse(summary->body).at("issues").at("open"), 2U);

    const auto repos = client.Get("/v1/repos");
    ASSERT_TRUE(repos != nullptr);
    EXPECT_EQ(repos->status, 200);
    EXPECT_EQ(nlohmann::json::parse(repos->body).at("repositories").size(), 2U);

    const auto repo = client.Get("/v1/repos/owner/repo");
    ASSERT_TRUE(repo != nullptr);
    EXPECT_EQ(repo->status, 200);
    EXPECT_EQ(nlohmann::json::parse(repo->body).at("repository").at("fullName"), "owner/repo");

    const auto issues = client.Get("/v1/issues?repo=other%2Frepo");
    ASSERT_TRUE(issues != nullptr);
    EXPECT_EQ(issues->status, 200);
    EXPECT_EQ(nlohmann::json::parse(issues->body).at("issues").size(), 1U);

    const auto pulls = client.Get("/v1/pulls");
    ASSERT_TRUE(pulls != nullptr);
    EXPECT_EQ(pulls->status, 200);
    EXPECT_EQ(nlohmann::json::parse(pulls->body).at("pullRequests").size(), 1U);

    const auto runs = client.Get("/v1/runs?status=queued");
    ASSERT_TRUE(runs != nullptr);
    EXPECT_EQ(runs->status, 200);
    EXPECT_EQ(nlohmann::json::parse(runs->body).at("workflowRuns").size(), 1U);

    const auto jobs = client.Get("/v1/jobs");
    ASSERT_TRUE(jobs != nullptr);
    EXPECT_EQ(jobs->status, 200);
    EXPECT_EQ(nlohmann::json::parse(jobs->body).at("jobs").size(), 1U);

    const auto activity = client.Get("/v1/activity");
    ASSERT_TRUE(activity != nullptr);
    EXPECT_EQ(activity->status, 200);
    EXPECT_EQ(nlohmann::json::parse(activity->body).at("activity").at("failedRuns").size(), 1U);

    const auto invalid = client.Get("/v1/runs?status=invalid");
    ASSERT_TRUE(invalid != nullptr);
    EXPECT_EQ(invalid->status, 400);

    const auto missing = client.Get("/v1/repos/unknown/repo");
    ASSERT_TRUE(missing != nullptr);
    EXPECT_EQ(missing->status, 404);

    api.stop();
    listener.join();
    EXPECT_TRUE(listen_succeeded.load(std::memory_order_acquire));
}

} // namespace
