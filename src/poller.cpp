#include "ghinfo/poller.hpp"

#include "ghinfo/snapshot_builder.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace ghinfo {
namespace {

constexpr std::uint64_t base_backoff_seconds = 5;
constexpr std::uint64_t max_backoff_seconds = 900;

std::uint64_t now_epoch_seconds() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now < 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(now);
}

std::string timestamp_after(std::chrono::seconds delay) {
    const auto time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + delay);
    std::tm utc{};
    if (gmtime_r(&time, &utc) == nullptr) {
        return utc_now_iso8601();
    }

    char buffer[32]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        return utc_now_iso8601();
    }
    return std::string{buffer};
}

std::string error_category(GitHubErrorKind kind) {
    switch (kind) {
    case GitHubErrorKind::transport:
        return "transport";
    case GitHubErrorKind::http:
        return "http";
    case GitHubErrorKind::malformed_json:
        return "malformed_json";
    case GitHubErrorKind::semantic:
        return "semantic";
    }
    return "unknown";
}

void wait_for_delay(std::stop_token stop_token, std::chrono::seconds delay) {
    std::mutex wait_mutex;
    std::condition_variable_any wait_condition;
    std::unique_lock lock{wait_mutex};
    wait_condition.wait_for(lock, stop_token, delay, [] { return false; });
}

} // namespace

std::chrono::seconds calculate_backoff(std::uint32_t consecutive_failures, bool rate_limited,
                                       std::optional<std::uint64_t> retry_after_seconds,
                                       std::optional<std::uint64_t> reset_epoch_seconds,
                                       std::uint64_t current_epoch_seconds, GitHubErrorKind kind) {
    if (rate_limited && retry_after_seconds.has_value()) {
        const auto bounded = std::min(retry_after_seconds.value(), max_backoff_seconds);
        return std::chrono::seconds{std::max<std::uint64_t>(bounded, 1)};
    }
    if (rate_limited && reset_epoch_seconds.has_value() &&
        reset_epoch_seconds.value() > current_epoch_seconds) {
        const auto bounded =
            std::min(reset_epoch_seconds.value() - current_epoch_seconds, max_backoff_seconds);
        return std::chrono::seconds{std::max<std::uint64_t>(bounded, 1)};
    }

    const auto maximum = kind == GitHubErrorKind::transport ? 60U : max_backoff_seconds;
    std::uint64_t delay = base_backoff_seconds;
    const auto attempts = std::max<std::uint32_t>(consecutive_failures, 1) - 1;
    for (std::uint32_t attempt = 0; attempt < attempts && delay < maximum; ++attempt) {
        delay = std::min(delay * 2, maximum);
    }
    return std::chrono::seconds{delay};
}

std::string format_poll_failure(const GitHubRequestError& error, std::uint32_t consecutive_failures,
                                std::chrono::seconds delay) {
    auto message = "poll failed (" + error_category(error.kind()) + ")";
    if (const auto status = error.status_code()) {
        message += " http_status=" + std::to_string(*status);
    }
    if (const auto code = error.transport_code()) {
        message += " curl_code=" + std::to_string(*code);
        message += " transport_reason=\"";
        message += curl_easy_strerror(static_cast<CURLcode>(*code));
        message += '"';
    }
    // Never log what(): exception messages may contain untrusted paths or payload fields.
    return message + " consecutive_failures=" + std::to_string(consecutive_failures) +
           " retry_in_seconds=" + std::to_string(delay.count());
}

Poller::Poller(const Config& config, GitHubClient& github, SnapshotStore& store)
    : config_(config), github_(github), store_(store) {}

void Poller::run(std::stop_token stop_token) {
    std::uint32_t consecutive_failures = 0;
    while (!stop_token.stop_requested()) {
        const auto attempt_timestamp = utc_now_iso8601();
        store_.record_poll_attempt(attempt_timestamp);

        try {
            refresh_once(attempt_timestamp);
            store_.record_poll_success(attempt_timestamp);
            if (consecutive_failures != 0) {
                std::cerr << utc_now_iso8601()
                          << " poll recovered after_failures=" << consecutive_failures
                          << " generation=" << store_.get()->generation << '\n';
            }
            consecutive_failures = 0;
            wait_for_delay(stop_token, std::chrono::seconds{config_.poll_interval_seconds});
        } catch (const GitHubRequestError& error) {
            if (consecutive_failures < std::numeric_limits<std::uint32_t>::max()) {
                ++consecutive_failures;
            }
            const auto status = error.status_code();
            const bool rate_limited =
                status.has_value() && (status.value() == 403 || status.value() == 429);
            const auto delay = calculate_backoff(
                consecutive_failures, rate_limited, github_.retry_after_seconds(),
                github_.rate_limit_reset_epoch_seconds(), now_epoch_seconds(), error.kind());
            store_.record_poll_failure(attempt_timestamp, error_category(error.kind()),
                                       timestamp_after(delay));
            std::cerr << utc_now_iso8601() << ' '
                      << format_poll_failure(error, consecutive_failures, delay) << '\n';
            wait_for_delay(stop_token, delay);
        } catch (const std::exception&) {
            if (consecutive_failures < std::numeric_limits<std::uint32_t>::max()) {
                ++consecutive_failures;
            }
            const auto delay = calculate_backoff(consecutive_failures, false, std::nullopt,
                                                 std::nullopt, now_epoch_seconds());
            store_.record_poll_failure(attempt_timestamp, "unexpected", timestamp_after(delay));
            std::cerr << utc_now_iso8601()
                      << " poll failed (unexpected) consecutive_failures=" << consecutive_failures
                      << " retry_in_seconds=" << delay.count() << '\n';
            wait_for_delay(stop_token, delay);
        }
    }
}

void Poller::refresh_once(std::string timestamp) {
    const auto current = store_.get();
    const auto generation = current == nullptr ? 1U : current->generation + 1U;
    auto candidate = std::make_shared<Snapshot>(
        build_snapshot(config_, github_, generation, std::move(timestamp)));
    store_.publish(std::move(candidate));
}

} // namespace ghinfo
