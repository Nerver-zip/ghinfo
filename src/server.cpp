#include "ghinfo/server.hpp"

#include <nlohmann/json.hpp>

#include <limits>
#include <memory>
#include <utility>

namespace ghinfo {

std::shared_ptr<const Snapshot> SnapshotStore::get() const {
    std::lock_guard lock{mutex_};
    return snapshot_;
}

bool SnapshotStore::ready() const {
    return static_cast<bool>(get());
}

void SnapshotStore::publish(std::shared_ptr<const Snapshot> snapshot) {
    if (!snapshot) {
        return;
    }

    std::lock_guard lock{mutex_};
    snapshot_ = std::move(snapshot);
}

void SnapshotStore::record_poll_attempt(std::string timestamp) {
    std::lock_guard lock{mutex_};
    poll_state_.last_attempt = std::move(timestamp);
}

void SnapshotStore::record_poll_success(std::string timestamp) {
    std::lock_guard lock{mutex_};
    poll_state_.last_attempt = timestamp;
    poll_state_.last_successful = std::move(timestamp);
    poll_state_.stale = false;
    poll_state_.consecutive_failures = 0;
    poll_state_.last_error_kind.reset();
    poll_state_.next_retry_at.reset();
}

void SnapshotStore::record_poll_failure(std::string timestamp, std::string error_kind,
                                        std::string next_retry_at) {
    std::lock_guard lock{mutex_};
    poll_state_.last_attempt = std::move(timestamp);
    poll_state_.stale = true;
    if (poll_state_.consecutive_failures < std::numeric_limits<std::uint32_t>::max()) {
        ++poll_state_.consecutive_failures;
    }
    poll_state_.last_error_kind = std::move(error_kind);
    poll_state_.next_retry_at = std::move(next_retry_at);
}

PollState SnapshotStore::poll_state() const {
    std::lock_guard lock{mutex_};
    return poll_state_;
}

JsonResponse make_health_response() {
    const nlohmann::json body{{"status", "ok"}};
    return JsonResponse{200, body.dump()};
}

JsonResponse make_readiness_response(bool ready) {
    const nlohmann::json body{{"ready", ready}};
    return JsonResponse{ready ? 200 : 503, body.dump()};
}

JsonResponse make_meta_response(bool snapshot_available) {
    const nlohmann::json body{
        {"schemaVersion", 1},
        {"service", "ghinfo"},
        {"version", GHINFO_VERSION},
        {"snapshotAvailable", snapshot_available},
    };
    return JsonResponse{200, body.dump()};
}

ApiServer::ApiServer(const Config& config, const SnapshotStore& store)
    : config_(config), store_(store) {
    register_routes();
}

void ApiServer::register_routes() {
    server_.Get("/healthz", [](const httplib::Request&, httplib::Response& response) {
        const auto payload = make_health_response();
        response.status = payload.status;
        response.set_content(payload.body, "application/json");
    });

    server_.Get("/readyz", [this](const httplib::Request&, httplib::Response& response) {
        const auto payload = make_readiness_response(store_.ready());
        response.status = payload.status;
        response.set_content(payload.body, "application/json");
    });

    server_.Get("/v1/meta", [this](const httplib::Request&, httplib::Response& response) {
        const auto payload = make_meta_response(store_.ready());
        response.status = payload.status;
        response.set_content(payload.body, "application/json");
    });
}

bool ApiServer::listen() {
    return server_.listen(config_.bind_address, static_cast<int>(config_.port));
}

void ApiServer::stop() {
    server_.stop();
}

} // namespace ghinfo
