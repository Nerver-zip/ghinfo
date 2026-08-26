#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>

namespace ghinfo {

[[nodiscard]] std::chrono::seconds
calculate_backoff(std::uint32_t consecutive_failures, bool rate_limited,
                  std::optional<std::uint64_t> retry_after_seconds,
                  std::optional<std::uint64_t> reset_epoch_seconds,
                  std::uint64_t now_epoch_seconds);

class Poller {
  public:
    Poller(const Config& config, GitHubClient& github, SnapshotStore& store);

    // MVP-010 will turn this boundary into the periodic std::jthread loop.
    void run(std::stop_token stop_token);

  private:
    void refresh_once(std::string timestamp);

    const Config& config_;
    GitHubClient& github_;
    SnapshotStore& store_;
};

} // namespace ghinfo
