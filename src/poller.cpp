#include "ghinfo/poller.hpp"

namespace ghinfo {

Poller::Poller(const Config& config, GitHubClient& github, SnapshotStore& store)
    : config_(config), github_(github), store_(store) {}

void Poller::run(std::stop_token stop_token) {
    // Scaffold only. MVP-010 implements the periodic loop after the GitHub resource
    // client and snapshot builder exist.
    (void)stop_token;
    (void)config_;
    (void)github_;
    (void)store_;
}

} // namespace ghinfo
