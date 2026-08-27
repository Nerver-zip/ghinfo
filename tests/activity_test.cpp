#include "ghinfo/activity.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace {

TEST(ActivityTest, ClassifiesItemsAndOrdersByPriorityThenRecency) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-26T20:00:00Z";
    snapshot.workflow_runs = {
        ghinfo::WorkflowRun{3001, "owner/repo", "CI", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "sha-1", "push",
                            "2026-08-26T12:00:00Z", "2026-08-26T12:03:00Z",
                            "https://github.com/owner/repo/actions/runs/3001"},
    };
    snapshot.jobs = {
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "gcc-debug", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-08-26T12:00:10Z"},
                            std::string{"2026-08-26T12:02:55Z"},
                            "https://github.com/owner/repo/actions/runs/3001/job/4001"},
        ghinfo::WorkflowJob{4002, 3002, "owner/repo", "clang-check", ghinfo::RunStatus::in_progress,
                            std::nullopt, std::string{"2026-08-26T14:00:00Z"}, std::nullopt,
                            "https://github.com/owner/repo/actions/runs/3002/job/4002"},
        ghinfo::WorkflowJob{4003, 3003, "owner/repo", "successful-check",
                            ghinfo::RunStatus::completed, ghinfo::Conclusion::success,
                            std::string{"2026-08-26T19:00:00Z"},
                            std::string{"2026-08-26T19:01:00Z"},
                            "https://github.com/owner/repo/actions/runs/3003/job/4003"},
    };
    snapshot.pull_requests = {
        ghinfo::PullRequest{2001, 17, "owner/repo", "Example pull request", "octocat", false,
                            "feature/example", "main", "2026-08-26T12:00:00Z",
                            "2026-08-26T15:00:00Z", "https://github.com/owner/repo/pull/17"},
    };
    snapshot.issues = {
        ghinfo::Issue{1001,
                      42,
                      "owner/repo",
                      "Example issue",
                      "octocat",
                      {},
                      "2026-08-26T12:00:00Z",
                      "2026-08-26T16:00:00Z",
                      "https://github.com/owner/repo/issues/42"},
    };

    const auto items = ghinfo::build_activity_items(snapshot);

    ASSERT_EQ(items.size(), 5U);
    EXPECT_EQ(items[0].kind, ghinfo::ActivityKind::failed_run);
    EXPECT_EQ(items[0].priority, ghinfo::ActivityPriority::critical);
    EXPECT_EQ(items[0].signals, std::vector<std::string>{"failed_run"});
    EXPECT_EQ(items[0].name, std::optional<std::string>{"CI"});
    EXPECT_EQ(items[1].kind, ghinfo::ActivityKind::failed_job);
    EXPECT_EQ(items[1].priority, ghinfo::ActivityPriority::critical);
    EXPECT_EQ(items[1].run_id, std::optional<ghinfo::GithubId>{3001});
    EXPECT_EQ(items[2].kind, ghinfo::ActivityKind::pull_request);
    EXPECT_EQ(items[2].priority, ghinfo::ActivityPriority::high);
    EXPECT_EQ(items[2].title, std::optional<std::string>{"Example pull request"});
    EXPECT_EQ(items[3].kind, ghinfo::ActivityKind::running_job);
    EXPECT_EQ(items[3].priority, ghinfo::ActivityPriority::high);
    EXPECT_EQ(items[3].name, std::optional<std::string>{"clang-check"});
    EXPECT_EQ(items[4].kind, ghinfo::ActivityKind::issue);
    EXPECT_EQ(items[4].priority, ghinfo::ActivityPriority::normal);
    EXPECT_EQ(items[4].title, std::optional<std::string>{"Example issue"});
}

TEST(ActivityTest, UsesRepositoryKindAndIdAsDeterministicTieBreakers) {
    ghinfo::Snapshot snapshot;
    snapshot.issues = {
        ghinfo::Issue{1002, 2, "b/repo", "B", "user", {}, "", "2026-08-26T12:00:00Z", ""},
        ghinfo::Issue{1004, 4, "a/repo", "A2", "user", {}, "", "2026-08-26T12:00:00Z", ""},
        ghinfo::Issue{1003, 3, "a/repo", "A1", "user", {}, "", "2026-08-26T12:00:00Z", ""},
    };

    const auto items = ghinfo::build_activity_items(snapshot);

    ASSERT_EQ(items.size(), 3U);
    EXPECT_EQ(items[0].repository, "a/repo");
    EXPECT_EQ(items[0].id, 1003U);
    EXPECT_EQ(items[1].repository, "a/repo");
    EXPECT_EQ(items[1].id, 1004U);
    EXPECT_EQ(items[2].repository, "b/repo");
}

TEST(ActivityTest, PlacesUntimestampedJobsAfterTimestampedItems) {
    ghinfo::Snapshot snapshot;
    snapshot.jobs = {
        ghinfo::WorkflowJob{4002, 3002, "owner/repo", "without-time",
                            ghinfo::RunStatus::in_progress, std::nullopt, std::nullopt,
                            std::nullopt, "https://github.com/owner/repo/job/4002"},
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "with-time", ghinfo::RunStatus::in_progress,
                            std::nullopt, std::string{"2026-08-26T12:00:00Z"}, std::nullopt,
                            "https://github.com/owner/repo/job/4001"},
    };

    const auto items = ghinfo::build_activity_items(snapshot);

    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].id, 4001U);
    EXPECT_EQ(items[0].updated_at, std::optional<std::string>{"2026-08-26T12:00:00Z"});
    EXPECT_EQ(items[1].id, 4002U);
    EXPECT_FALSE(items[1].updated_at.has_value());
}

} // namespace
