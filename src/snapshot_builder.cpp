#include "ghinfo/snapshot_builder.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ghinfo {

Snapshot build_snapshot(const Config& config, const GitHubClient& github, std::uint64_t generation,
                        std::string generated_at) {
    Snapshot snapshot;
    snapshot.generation = generation;
    snapshot.generated_at = std::move(generated_at);
    snapshot.last_successful_poll = snapshot.generated_at;

    for (const auto& repository_ref : config.repositories) {
        snapshot.repositories.push_back(github.fetch_repository(repository_ref));

        auto issues = github.fetch_open_issues(repository_ref);
        snapshot.issues.insert(snapshot.issues.end(), std::make_move_iterator(issues.begin()),
                               std::make_move_iterator(issues.end()));

        auto pull_requests = github.fetch_open_pull_requests(repository_ref);
        snapshot.pull_requests.insert(snapshot.pull_requests.end(),
                                      std::make_move_iterator(pull_requests.begin()),
                                      std::make_move_iterator(pull_requests.end()));

        auto workflow_runs = github.fetch_workflow_runs(repository_ref, config.run_history);
        for (const auto& run : workflow_runs) {
            auto jobs = github.fetch_relevant_workflow_jobs(repository_ref, run);
            snapshot.jobs.insert(snapshot.jobs.end(), std::make_move_iterator(jobs.begin()),
                                 std::make_move_iterator(jobs.end()));
        }
        snapshot.workflow_runs.insert(snapshot.workflow_runs.end(),
                                      std::make_move_iterator(workflow_runs.begin()),
                                      std::make_move_iterator(workflow_runs.end()));
    }

    std::sort(snapshot.repositories.begin(), snapshot.repositories.end(),
              [](const Repository& left, const Repository& right) {
                  return left.full_name < right.full_name;
              });
    std::sort(snapshot.issues.begin(), snapshot.issues.end(),
              [](const Issue& left, const Issue& right) {
                  return std::tie(left.repository, left.number) <
                         std::tie(right.repository, right.number);
              });
    std::sort(snapshot.pull_requests.begin(), snapshot.pull_requests.end(),
              [](const PullRequest& left, const PullRequest& right) {
                  return std::tie(left.repository, left.number) <
                         std::tie(right.repository, right.number);
              });
    std::sort(snapshot.workflow_runs.begin(), snapshot.workflow_runs.end(),
              [](const WorkflowRun& left, const WorkflowRun& right) {
                  return std::tie(left.repository, left.id) < std::tie(right.repository, right.id);
              });
    std::sort(snapshot.jobs.begin(), snapshot.jobs.end(),
              [](const WorkflowJob& left, const WorkflowJob& right) {
                  return std::tie(left.repository, left.id) < std::tie(right.repository, right.id);
              });
    snapshot.rate_limit = github.rate_limit();
    return snapshot;
}

std::string utc_now_iso8601() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    if (gmtime_r(&time, &utc) == nullptr) {
        throw std::runtime_error("failed to format current UTC time");
    }

    char buffer[32]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        throw std::runtime_error("failed to format current UTC time");
    }
    return std::string{buffer};
}

} // namespace ghinfo
