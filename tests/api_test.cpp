#include "ghinfo/server.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

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

} // namespace
