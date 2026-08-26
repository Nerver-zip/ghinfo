#include "ghinfo/poller.hpp"

#include "ghinfo/snapshot_builder.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

namespace ghinfo {

Poller::Poller(const Config& config, GitHubClient& github, SnapshotStore& store)
    : config_(config), github_(github), store_(store) {}

void Poller::run(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        try {
            refresh_once();
        } catch (const std::exception& error) {
            std::cerr << "poll failed: " << error.what() << '\n';
        }

        std::mutex wait_mutex;
        std::condition_variable_any wait_condition;
        std::unique_lock lock{wait_mutex};
        const auto stopped = wait_condition.wait_for(
            lock, stop_token, std::chrono::seconds{config_.poll_interval_seconds},
            [] { return false; });
        if (stopped) {
            return;
        }
    }
}

void Poller::refresh_once() {
    const auto current = store_.get();
    const auto generation = current == nullptr ? 1U : current->generation + 1U;
    auto candidate =
        std::make_shared<Snapshot>(build_snapshot(config_, github_, generation, utc_now_iso8601()));
    store_.publish(std::move(candidate));
}

} // namespace ghinfo
