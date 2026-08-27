#include "ghinfo/activity.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

namespace ghinfo {
namespace {

[[nodiscard]] ActivityItem base_item(ActivityKind kind, ActivityPriority priority,
                                     std::string signal, std::string repository, GithubId id,
                                     std::optional<std::string> updated_at, std::string url) {
    ActivityItem item;
    item.kind = kind;
    item.priority = priority;
    item.signals.push_back(std::move(signal));
    item.repository = std::move(repository);
    item.id = id;
    item.updated_at = std::move(updated_at);
    item.url = std::move(url);
    return item;
}

[[nodiscard]] int priority_rank(ActivityPriority priority) {
    switch (priority) {
    case ActivityPriority::critical:
        return 0;
    case ActivityPriority::high:
        return 1;
    case ActivityPriority::normal:
        return 2;
    }
    return 2;
}

[[nodiscard]] std::optional<std::string> job_timestamp(const WorkflowJob& job) {
    if (job.completed_at.has_value()) {
        return job.completed_at;
    }
    return job.started_at;
}

[[nodiscard]] bool is_active(const WorkflowJob& job) {
    return job.status == RunStatus::queued || job.status == RunStatus::in_progress;
}

[[nodiscard]] bool activity_less(const ActivityItem& left, const ActivityItem& right) {
    if (priority_rank(left.priority) != priority_rank(right.priority)) {
        return priority_rank(left.priority) < priority_rank(right.priority);
    }

    if (left.updated_at.has_value() != right.updated_at.has_value()) {
        return left.updated_at.has_value();
    }
    if (left.updated_at.has_value() && left.updated_at != right.updated_at) {
        return *left.updated_at > *right.updated_at;
    }

    return std::tuple{left.repository, to_string(left.kind), left.id} <
           std::tuple{right.repository, to_string(right.kind), right.id};
}

} // namespace

std::vector<ActivityItem> build_activity_items(const Snapshot& snapshot) {
    std::vector<ActivityItem> items;
    items.reserve(snapshot.jobs.size() + snapshot.workflow_runs.size() +
                  snapshot.pull_requests.size() + snapshot.issues.size());

    for (const auto& job : snapshot.jobs) {
        if (job.conclusion == Conclusion::failure) {
            auto item =
                base_item(ActivityKind::failed_job, ActivityPriority::critical, "failed_job",
                          job.repository, job.id, job_timestamp(job), job.url);
            item.run_id = job.run_id;
            item.name = job.name;
            item.status = job.status;
            item.conclusion = job.conclusion;
            items.push_back(std::move(item));
        } else if (is_active(job)) {
            auto item = base_item(ActivityKind::running_job, ActivityPriority::high, "running_job",
                                  job.repository, job.id, job_timestamp(job), job.url);
            item.run_id = job.run_id;
            item.name = job.name;
            item.status = job.status;
            items.push_back(std::move(item));
        }
    }

    for (const auto& run : snapshot.workflow_runs) {
        if (run.conclusion == Conclusion::failure) {
            auto item = base_item(ActivityKind::failed_run, ActivityPriority::critical,
                                  "failed_run", run.repository, run.id, run.updated_at, run.url);
            item.name = run.name;
            item.status = run.status;
            item.conclusion = run.conclusion;
            items.push_back(std::move(item));
        }
    }

    for (const auto& pull_request : snapshot.pull_requests) {
        auto item = base_item(ActivityKind::pull_request, ActivityPriority::high,
                              "open_pull_request", pull_request.repository, pull_request.id,
                              pull_request.updated_at, pull_request.url);
        item.number = pull_request.number;
        item.title = pull_request.title;
        items.push_back(std::move(item));
    }

    for (const auto& issue : snapshot.issues) {
        auto item = base_item(ActivityKind::issue, ActivityPriority::normal, "open_issue",
                              issue.repository, issue.id, issue.updated_at, issue.url);
        item.number = issue.number;
        item.title = issue.title;
        items.push_back(std::move(item));
    }

    std::sort(items.begin(), items.end(), activity_less);
    return items;
}

} // namespace ghinfo
