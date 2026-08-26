#pragma once

#include "ghinfo/model.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ghinfo {

struct RateLimit {
    std::uint32_t limit{};
    std::uint32_t remaining{};
    std::optional<std::string> reset_at;
};

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

class SnapshotStore {
  public:
    [[nodiscard]] std::shared_ptr<const Snapshot> get() const;
    [[nodiscard]] bool ready() const;
    void publish(std::shared_ptr<const Snapshot> snapshot);

  private:
    mutable std::mutex mutex_;
    std::shared_ptr<const Snapshot> snapshot_;
};

} // namespace ghinfo
