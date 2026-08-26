#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/snapshot.hpp"

#include <cstdint>
#include <stop_token>

namespace ghinfo {

class Poller {
  public:
    Poller(const Config& config, GitHubClient& github, SnapshotStore& store);

    // MVP-010 will turn this boundary into the periodic std::jthread loop.
    void run(std::stop_token stop_token);

  private:
    void refresh_once();

    const Config& config_;
    GitHubClient& github_;
    SnapshotStore& store_;
};

} // namespace ghinfo
