#pragma once

#include "ghinfo/model.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ghinfo {

struct Snapshot {
    std::uint64_t generation{};
    std::string generated_at;
    std::string last_successful_poll;
    std::vector<Repository> repositories;
    std::vector<Issue> issues;
    std::vector<PullRequest> pull_requests;
    std::vector<WorkflowRun> workflow_runs;
    std::vector<WorkflowJob> jobs;
    std::optional<RateLimit> rate_limit;
};

struct PollState {
    std::optional<std::string> last_attempt;
    std::optional<std::string> last_successful;
    bool stale{true};
    std::uint32_t consecutive_failures{};
    std::optional<std::string> last_error_kind;
    std::optional<std::string> next_retry_at;
};

class SnapshotStore {
  public:
    [[nodiscard]] std::shared_ptr<const Snapshot> get() const;
    [[nodiscard]] bool ready() const;
    void publish(std::shared_ptr<const Snapshot> snapshot);
    void record_poll_attempt(std::string timestamp);
    void record_poll_success(std::string timestamp);
    void record_poll_failure(std::string timestamp, std::string error_kind,
                             std::string next_retry_at);
    [[nodiscard]] PollState poll_state() const;

  private:
    mutable std::mutex mutex_;
    std::shared_ptr<const Snapshot> snapshot_;
    PollState poll_state_;
};

} // namespace ghinfo
