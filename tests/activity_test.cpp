#include "ghinfo/activity.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>

namespace {

TEST(ActivityTest, ParsesSupportedCategoriesAndRejectsUnknownValues) {
    EXPECT_EQ(ghinfo::parse_activity_category("workflows"), ghinfo::ActivityCategory::workflows);
    EXPECT_EQ(ghinfo::parse_activity_category("pull_requests"),
              ghinfo::ActivityCategory::pull_requests);
    EXPECT_EQ(ghinfo::parse_activity_category("issues"), ghinfo::ActivityCategory::issues);
    EXPECT_FALSE(ghinfo::parse_activity_category("unknown").has_value());
    EXPECT_FALSE(ghinfo::parse_activity_category("").has_value());
}

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
    const std::vector<std::string> recent_run_signals{"failed_run", "recent_failure"};
    EXPECT_EQ(items[0].signals, recent_run_signals);
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

TEST(ActivityTest, AgesFailuresAndExpiresThemUsingSnapshotTime) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-28T00:00:00Z";
    snapshot.workflow_runs = {
        ghinfo::WorkflowRun{3001, "owner/repo", "recent", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "sha-1", "push",
                            "2026-08-27T00:00:00Z", "2026-08-21T00:00:00Z", "run-3001"},
        ghinfo::WorkflowRun{3002, "owner/repo", "stale", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "sha-2", "push",
                            "2026-08-01T00:00:00Z", "2026-08-01T00:00:00Z", "run-3002"},
        ghinfo::WorkflowRun{3003, "owner/repo", "expired", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "sha-3", "push",
                            "2026-07-01T00:00:00Z", "2026-07-01T00:00:00Z", "run-3003"},
    };
    snapshot.jobs = {
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "recent-job", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-08-20T00:00:00Z"},
                            std::string{"2026-08-27T00:00:00Z"}, "job-4001"},
        ghinfo::WorkflowJob{4002, 3002, "owner/repo", "stale-job", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-08-01T00:00:00Z"},
                            std::nullopt, "job-4002"},
        ghinfo::WorkflowJob{4003, 3003, "owner/repo", "expired-job", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-07-01T00:00:00Z"},
                            std::string{"2026-07-01T00:00:00Z"}, "job-4003"},
    };

    const auto items = ghinfo::build_activity_items(snapshot);

    ASSERT_EQ(items.size(), 4U);
    const auto recent_run =
        std::find_if(items.begin(), items.end(), [](const auto& item) { return item.id == 3001U; });
    ASSERT_NE(recent_run, items.end());
    EXPECT_EQ(recent_run->priority, ghinfo::ActivityPriority::critical);
    const std::vector<std::string> recent_run_signals{"failed_run", "recent_failure"};
    EXPECT_EQ(recent_run->signals, recent_run_signals);

    const auto stale_run =
        std::find_if(items.begin(), items.end(), [](const auto& item) { return item.id == 3002U; });
    ASSERT_NE(stale_run, items.end());
    EXPECT_EQ(stale_run->priority, ghinfo::ActivityPriority::normal);
    const std::vector<std::string> stale_run_signals{"failed_run", "stale_failure"};
    EXPECT_EQ(stale_run->signals, stale_run_signals);

    const auto stale_job =
        std::find_if(items.begin(), items.end(), [](const auto& item) { return item.id == 4002U; });
    ASSERT_NE(stale_job, items.end());
    EXPECT_EQ(stale_job->priority, ghinfo::ActivityPriority::normal);
    const std::vector<std::string> stale_job_signals{"failed_job", "stale_failure"};
    EXPECT_EQ(stale_job->signals, stale_job_signals);
    EXPECT_EQ(std::count_if(items.begin(), items.end(),
                            [](const auto& item) { return item.id == 3003U || item.id == 4003U; }),
              0);
}

TEST(ActivityTest, KeepsRunningWorkflowsAndJobsRelevant) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-28T00:00:00Z";
    snapshot.workflow_runs = {
        ghinfo::WorkflowRun{3001, "owner/repo", "running", ghinfo::RunStatus::in_progress,
                            std::nullopt, "main", "sha-1", "push", "2026-07-01T00:00:00Z",
                            "2026-07-01T00:00:00Z", "run-3001"},
    };
    snapshot.jobs = {
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "running-job", ghinfo::RunStatus::queued,
                            std::nullopt, std::nullopt, std::nullopt, "job-4001"},
    };

    const auto items = ghinfo::build_activity_items(snapshot);

    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].kind, ghinfo::ActivityKind::running_run);
    EXPECT_EQ(items[1].kind, ghinfo::ActivityKind::running_job);
    EXPECT_EQ(items[0].priority, ghinfo::ActivityPriority::high);
    EXPECT_EQ(items[1].priority, ghinfo::ActivityPriority::high);
}

TEST(ActivityTest, SelectsBalancedCategoriesAndRedistributesMissingCategories) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-28T00:00:00Z";
    for (ghinfo::GithubId id = 1; id <= 6; ++id) {
        snapshot.jobs.push_back(ghinfo::WorkflowJob{
            4000 + id, 3000 + id, "owner/repo", "job", ghinfo::RunStatus::in_progress, std::nullopt,
            std::string{"2026-08-27T" + std::to_string(10 + id) + ":00:00Z"}, std::nullopt, "job"});
        snapshot.pull_requests.push_back(ghinfo::PullRequest{
            5000 + id, id, "owner/repo", "pr", "user", false, "head", "main",
            "2026-08-27T00:00:00Z", "2026-08-27T" + std::to_string(10 + id) + ":00:00Z", "pr"});
        snapshot.issues.push_back(ghinfo::Issue{6000 + id,
                                                id,
                                                "owner/repo",
                                                "issue",
                                                "user",
                                                {},
                                                "2026-08-27T00:00:00Z",
                                                "2026-08-27T" + std::to_string(10 + id) + ":00:00Z",
                                                "issue"});
    }
    const auto items = ghinfo::build_activity_items(snapshot);

    const auto count_category = [](const std::vector<ghinfo::ActivityItem>& selected,
                                   int expected_category) {
        return std::count_if(
            selected.begin(), selected.end(), [expected_category](const auto& item) {
                const auto is_work = item.kind == ghinfo::ActivityKind::running_job ||
                                     item.kind == ghinfo::ActivityKind::running_run ||
                                     item.kind == ghinfo::ActivityKind::failed_job ||
                                     item.kind == ghinfo::ActivityKind::failed_run;
                const auto actual = is_work                                           ? 0
                                    : item.kind == ghinfo::ActivityKind::pull_request ? 1
                                                                                      : 2;
                return actual == expected_category;
            });
    };

    EXPECT_EQ(ghinfo::select_activity_items(items, 1).size(), 1U);
    EXPECT_EQ(ghinfo::select_activity_items(items, 2).size(), 2U);
    for (const auto limit : {3U, 4U, 5U}) {
        const auto selected = ghinfo::select_activity_items(items, limit);
        EXPECT_EQ(selected.size(), limit);
        EXPECT_GE(count_category(selected, 0), 1);
        EXPECT_GE(count_category(selected, 1), 1);
        EXPECT_GE(count_category(selected, 2), 1);
    }
    const auto six = ghinfo::select_activity_items(items, 6);
    EXPECT_EQ(count_category(six, 0), 2);
    EXPECT_EQ(count_category(six, 1), 2);
    EXPECT_EQ(count_category(six, 2), 2);
    const auto eight = ghinfo::select_activity_items(items, 8);
    EXPECT_EQ(eight.size(), 8U);
    EXPECT_GE(count_category(eight, 0), 2);
    EXPECT_GE(count_category(eight, 1), 2);
    EXPECT_GE(count_category(eight, 2), 2);

    snapshot.issues.clear();
    const auto without_issues = ghinfo::build_activity_items(snapshot);
    const auto redistributed = ghinfo::select_activity_items(without_issues, 6);
    EXPECT_EQ(redistributed.size(), 6U);
    EXPECT_EQ(count_category(redistributed, 2), 0);
}

TEST(ActivityTest, SelectsOnlyRequestedCategoryInExistingActivityOrder) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-28T00:00:00Z";
    for (ghinfo::GithubId id = 1; id <= 4; ++id) {
        snapshot.jobs.push_back(ghinfo::WorkflowJob{
            4000 + id, 3000 + id, "owner/repo", "job", ghinfo::RunStatus::in_progress, std::nullopt,
            "2026-08-27T" + std::to_string(10 + id) + ":00:00Z", std::nullopt, "job"});
        snapshot.pull_requests.push_back(
            ghinfo::PullRequest{5000 + id, id, "owner/repo", "pr", "user", false, "head", "main",
                                "", "2026-08-27T" + std::to_string(10 + id) + ":00:00Z", "pr"});
        snapshot.issues.push_back(ghinfo::Issue{6000 + id,
                                                id,
                                                "owner/repo",
                                                "issue",
                                                "user",
                                                {},
                                                "",
                                                "2026-08-27T" + std::to_string(10 + id) + ":00:00Z",
                                                "issue"});
    }

    const auto items = ghinfo::build_activity_items(snapshot);
    const auto workflows =
        ghinfo::select_activity_items(items, 3, ghinfo::ActivityCategory::workflows);
    const auto pull_requests =
        ghinfo::select_activity_items(items, 3, ghinfo::ActivityCategory::pull_requests);
    const auto issues = ghinfo::select_activity_items(items, 3, ghinfo::ActivityCategory::issues);

    ASSERT_EQ(workflows.size(), 3U);
    EXPECT_EQ(workflows[0].id, 4004U);
    EXPECT_EQ(workflows[1].id, 4003U);
    EXPECT_EQ(workflows[2].id, 4002U);
    EXPECT_TRUE(std::all_of(workflows.begin(), workflows.end(), [](const auto& item) {
        return item.kind == ghinfo::ActivityKind::running_job;
    }));

    ASSERT_EQ(pull_requests.size(), 3U);
    EXPECT_TRUE(std::all_of(pull_requests.begin(), pull_requests.end(), [](const auto& item) {
        return item.kind == ghinfo::ActivityKind::pull_request;
    }));
    ASSERT_EQ(issues.size(), 3U);
    EXPECT_TRUE(std::all_of(issues.begin(), issues.end(), [](const auto& item) {
        return item.kind == ghinfo::ActivityKind::issue;
    }));
}

TEST(ActivityTest, AvoidsIncidentDuplicatesInTopThreeButKeepsBothAtLargerLimits) {
    ghinfo::Snapshot snapshot;
    snapshot.generated_at = "2026-08-28T00:00:00Z";
    snapshot.workflow_runs = {
        ghinfo::WorkflowRun{3001, "owner/repo", "CI", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, "main", "sha", "push",
                            "2026-08-27T00:00:00Z", "2026-08-27T00:00:00Z", "run"},
    };
    snapshot.jobs = {
        ghinfo::WorkflowJob{4001, 3001, "owner/repo", "gcc", ghinfo::RunStatus::completed,
                            ghinfo::Conclusion::failure, std::string{"2026-08-27T00:00:00Z"},
                            std::string{"2026-08-27T00:00:00Z"}, "job"},
    };
    snapshot.pull_requests = {
        ghinfo::PullRequest{2001, 1, "owner/repo", "PR", "user", false, "head", "main", "",
                            "2026-08-26T00:00:00Z", "pr"},
    };
    snapshot.issues = {
        ghinfo::Issue{
            1001, 1, "owner/repo", "Issue", "user", {}, "", "2026-08-25T00:00:00Z", "issue"},
    };
    const auto items = ghinfo::build_activity_items(snapshot);

    const auto top_two = ghinfo::select_activity_items(items, 2);
    ASSERT_EQ(top_two.size(), 2U);
    EXPECT_FALSE(top_two[0].kind == ghinfo::ActivityKind::failed_run &&
                 top_two[1].kind == ghinfo::ActivityKind::failed_job);
    EXPECT_FALSE(top_two[0].kind == ghinfo::ActivityKind::failed_job &&
                 top_two[1].kind == ghinfo::ActivityKind::failed_run);

    const auto top_six = ghinfo::select_activity_items(items, 6);
    ASSERT_EQ(top_six.size(), 4U);
    EXPECT_EQ(std::count_if(
                  top_six.begin(), top_six.end(),
                  [](const auto& item) { return item.kind == ghinfo::ActivityKind::failed_run; }),
              1);
    EXPECT_EQ(std::count_if(
                  top_six.begin(), top_six.end(),
                  [](const auto& item) { return item.kind == ghinfo::ActivityKind::failed_job; }),
              1);
    EXPECT_FALSE(top_six[0].kind == ghinfo::ActivityKind::failed_run &&
                 top_six[1].kind == ghinfo::ActivityKind::failed_job);

    snapshot.pull_requests.clear();
    snapshot.issues.clear();
    const auto only_incident = ghinfo::build_activity_items(snapshot);
    const auto without_alternative = ghinfo::select_activity_items(only_incident, 2);
    ASSERT_EQ(without_alternative.size(), 2U);
    EXPECT_EQ(without_alternative[0].kind, ghinfo::ActivityKind::failed_job);
    EXPECT_EQ(without_alternative[1].kind, ghinfo::ActivityKind::failed_run);
}

} // namespace
