#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ghinfo {

using GithubId = std::uint64_t;

struct RateLimit {
    std::uint32_t limit{};
    std::uint32_t remaining{};
    std::optional<std::string> reset_at;
};

enum class RunStatus {
    queued,
    in_progress,
    completed,
    unknown,
};

enum class Conclusion {
    success,
    failure,
    cancelled,
    skipped,
    timed_out,
    neutral,
    action_required,
    unknown,
};

enum class ActivityKind {
    failed_job,
    failed_run,
    running_job,
    pull_request,
    issue,
};

enum class ActivityPriority {
    critical,
    high,
    normal,
};

struct Repository {
    GithubId id{};
    std::string full_name;
    bool is_private{};
    std::string default_branch;
    std::string url;
    std::string updated_at;
};

struct Issue {
    GithubId id{};
    std::uint64_t number{};
    std::string repository;
    std::string title;
    std::string author;
    std::vector<std::string> labels;
    std::string created_at;
    std::string updated_at;
    std::string url;
};

struct PullRequest {
    GithubId id{};
    std::uint64_t number{};
    std::string repository;
    std::string title;
    std::string author;
    bool draft{};
    std::string head;
    std::string base;
    std::string created_at;
    std::string updated_at;
    std::string url;
};

struct WorkflowRun {
    GithubId id{};
    std::string repository;
    std::string name;
    RunStatus status{RunStatus::unknown};
    std::optional<Conclusion> conclusion;
    std::string branch;
    std::string commit_sha;
    std::string event;
    std::string created_at;
    std::string updated_at;
    std::string url;
};

struct WorkflowJob {
    GithubId id{};
    GithubId run_id{};
    std::string repository;
    std::string name;
    RunStatus status{RunStatus::unknown};
    std::optional<Conclusion> conclusion;
    std::optional<std::string> started_at;
    std::optional<std::string> completed_at;
    std::string url;
};

struct ActivityItem {
    ActivityKind kind{ActivityKind::issue};
    ActivityPriority priority{ActivityPriority::normal};
    std::vector<std::string> signals;
    std::string repository;
    GithubId id{};
    std::optional<std::uint64_t> number;
    std::optional<GithubId> run_id;
    std::optional<std::string> title;
    std::optional<std::string> name;
    std::optional<RunStatus> status;
    std::optional<Conclusion> conclusion;
    std::optional<std::string> updated_at;
    std::string url;
};

[[nodiscard]] std::string to_string(RunStatus status);
[[nodiscard]] std::string to_string(Conclusion conclusion);
[[nodiscard]] std::string to_string(ActivityKind kind);
[[nodiscard]] std::string to_string(ActivityPriority priority);

} // namespace ghinfo
