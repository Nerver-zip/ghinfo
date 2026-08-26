#include "ghinfo/server.hpp"

#include "ghinfo/config.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace ghinfo {
namespace {

using Json = nlohmann::json;

constexpr int schema_version = 1;

[[nodiscard]] Json repository_json(const Repository& repository) {
    return Json{{"id", repository.id},
                {"fullName", repository.full_name},
                {"private", repository.is_private},
                {"defaultBranch", repository.default_branch},
                {"url", repository.url},
                {"updatedAt", repository.updated_at}};
}

[[nodiscard]] Json issue_json(const Issue& issue) {
    return Json{{"id", issue.id},
                {"number", issue.number},
                {"repository", issue.repository},
                {"title", issue.title},
                {"author", issue.author},
                {"labels", issue.labels},
                {"createdAt", issue.created_at},
                {"updatedAt", issue.updated_at},
                {"url", issue.url}};
}

[[nodiscard]] Json pull_request_json(const PullRequest& pull_request) {
    return Json{{"id", pull_request.id},
                {"number", pull_request.number},
                {"repository", pull_request.repository},
                {"title", pull_request.title},
                {"author", pull_request.author},
                {"draft", pull_request.draft},
                {"head", pull_request.head},
                {"base", pull_request.base},
                {"createdAt", pull_request.created_at},
                {"updatedAt", pull_request.updated_at},
                {"url", pull_request.url}};
}

[[nodiscard]] Json nullable_string(const std::optional<std::string>& value) {
    return value.has_value() ? Json(*value) : Json(nullptr);
}

[[nodiscard]] Json nullable_conclusion(const std::optional<Conclusion>& value) {
    return value.has_value() ? Json(to_string(*value)) : Json(nullptr);
}

[[nodiscard]] Json workflow_run_json(const WorkflowRun& run) {
    return Json{{"id", run.id},
                {"repository", run.repository},
                {"name", run.name},
                {"status", to_string(run.status)},
                {"conclusion", nullable_conclusion(run.conclusion)},
                {"branch", run.branch},
                {"commitSha", run.commit_sha},
                {"event", run.event},
                {"createdAt", run.created_at},
                {"updatedAt", run.updated_at},
                {"url", run.url}};
}

[[nodiscard]] Json workflow_job_json(const WorkflowJob& job) {
    return Json{{"id", job.id},
                {"runId", job.run_id},
                {"repository", job.repository},
                {"name", job.name},
                {"status", to_string(job.status)},
                {"conclusion", nullable_conclusion(job.conclusion)},
                {"startedAt", nullable_string(job.started_at)},
                {"completedAt", nullable_string(job.completed_at)},
                {"url", job.url}};
}

[[nodiscard]] Json rate_limit_json(const RateLimit& rate_limit) {
    return Json{{"limit", rate_limit.limit},
                {"remaining", rate_limit.remaining},
                {"resetAt", nullable_string(rate_limit.reset_at)}};
}

[[nodiscard]] Json poll_state_json(const PollState& state) {
    return Json{{"lastAttempt", nullable_string(state.last_attempt)},
                {"lastSuccessful", nullable_string(state.last_successful)},
                {"stale", state.stale},
                {"consecutiveFailures", state.consecutive_failures},
                {"lastErrorKind", nullable_string(state.last_error_kind)},
                {"nextRetryAt", nullable_string(state.next_retry_at)}};
}

[[nodiscard]] Json base_json(const Snapshot& snapshot) {
    return Json{{"schemaVersion", schema_version},
                {"generation", snapshot.generation},
                {"generatedAt", snapshot.generated_at}};
}

[[nodiscard]] Json base_json(const SnapshotStore& store, const Snapshot& snapshot) {
    auto body = base_json(snapshot);
    body["stale"] = store.poll_state().stale;
    return body;
}

[[nodiscard]] JsonResponse json_response(const Json& body, int status = 200) {
    return JsonResponse{status, body.dump()};
}

[[nodiscard]] JsonResponse error_response(std::string_view error, int status) {
    return json_response(Json{{"schemaVersion", schema_version}, {"error", error}}, status);
}

[[nodiscard]] bool matches_repository(const std::optional<std::string>& repository,
                                      std::string_view item_repository) {
    return !repository.has_value() || *repository == item_repository;
}

[[nodiscard]] std::optional<RunStatus> parse_run_status(const std::string& value) {
    if (value == "queued") {
        return RunStatus::queued;
    }
    if (value == "in_progress") {
        return RunStatus::in_progress;
    }
    if (value == "completed") {
        return RunStatus::completed;
    }
    if (value == "unknown") {
        return RunStatus::unknown;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Conclusion> parse_conclusion(const std::string& value) {
    const std::array<std::pair<std::string_view, Conclusion>, 8> values{{
        {"success", Conclusion::success},
        {"failure", Conclusion::failure},
        {"cancelled", Conclusion::cancelled},
        {"skipped", Conclusion::skipped},
        {"timed_out", Conclusion::timed_out},
        {"neutral", Conclusion::neutral},
        {"action_required", Conclusion::action_required},
        {"unknown", Conclusion::unknown},
    }};
    for (const auto& [name, conclusion] : values) {
        if (value == name) {
            return conclusion;
        }
    }
    return std::nullopt;
}

void write_response(const JsonResponse& payload, httplib::Response& response) {
    response.status = payload.status;
    response.set_content(payload.body, "application/json");
}

[[nodiscard]] std::optional<std::string> query_param(const httplib::Request& request,
                                                     std::string_view name) {
    if (!request.has_param(std::string{name})) {
        return std::nullopt;
    }
    return request.get_param_value(std::string{name});
}

[[nodiscard]] std::optional<std::string> valid_repository_filter(const httplib::Request& request,
                                                                 JsonResponse& error) {
    const auto value = query_param(request, "repo");
    if (!value.has_value()) {
        return std::nullopt;
    }
    try {
        return parse_repository_ref(*value).full_name();
    } catch (const std::exception&) {
        error = error_response("invalid_repository_filter", 400);
        return std::nullopt;
    }
}

} // namespace

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
    return json_response(Json{{"status", "ok"}});
}

JsonResponse make_readiness_response(bool ready) {
    return json_response(Json{{"ready", ready}}, ready ? 200 : 503);
}

JsonResponse make_meta_response(bool snapshot_available) {
    return json_response(Json{{"schemaVersion", schema_version},
                              {"service", "ghinfo"},
                              {"version", GHINFO_VERSION},
                              {"snapshotAvailable", snapshot_available}});
}

JsonResponse make_meta_response(const SnapshotStore& store) {
    const auto snapshot = store.get();
    const auto state = store.poll_state();
    Json body{{"schemaVersion", schema_version},
              {"service", "ghinfo"},
              {"version", GHINFO_VERSION},
              {"snapshotAvailable", snapshot != nullptr},
              {"poll", poll_state_json(state)}};
    if (snapshot != nullptr) {
        body["generation"] = snapshot->generation;
        body["generatedAt"] = snapshot->generated_at;
        body["rateLimit"] = snapshot->rate_limit.has_value()
                                ? rate_limit_json(*snapshot->rate_limit)
                                : Json{nullptr};
    }
    return json_response(body);
}

JsonResponse make_summary_response(const SnapshotStore& store) {
    const auto snapshot = store.get();
    if (snapshot == nullptr) {
        return make_snapshot_unavailable_response();
    }

    const auto& value = *snapshot;
    std::uint64_t draft_pull_requests = 0;
    std::uint64_t queued_runs = 0;
    std::uint64_t running_runs = 0;
    std::uint64_t failed_runs = 0;
    for (const auto& pull_request : value.pull_requests) {
        if (pull_request.draft) {
            ++draft_pull_requests;
        }
    }
    for (const auto& run : value.workflow_runs) {
        if (run.status == RunStatus::queued) {
            ++queued_runs;
        }
        if (run.status == RunStatus::in_progress) {
            ++running_runs;
        }
        if (run.conclusion == Conclusion::failure) {
            ++failed_runs;
        }
    }

    auto body = base_json(store, value);
    body["repositories"] = Json{{"total", value.repositories.size()}};
    body["issues"] = Json{{"open", value.issues.size()}};
    body["pullRequests"] =
        Json{{"open", value.pull_requests.size()}, {"draft", draft_pull_requests}};
    body["actions"] =
        Json{{"queued", queued_runs}, {"running", running_runs}, {"failed", failed_runs}};
    return json_response(body);
}

JsonResponse make_repositories_response(const Snapshot& snapshot) {
    Json repositories = Json::array();
    for (const auto& repository : snapshot.repositories) {
        repositories.push_back(repository_json(repository));
    }
    auto body = base_json(snapshot);
    body["repositories"] = std::move(repositories);
    return json_response(body);
}

JsonResponse make_repository_response(const Snapshot& snapshot, const std::string& full_name) {
    const auto repository = std::find_if(
        snapshot.repositories.begin(), snapshot.repositories.end(),
        [&full_name](const Repository& value) { return value.full_name == full_name; });
    if (repository == snapshot.repositories.end()) {
        return error_response("repository_not_found", 404);
    }

    auto body = base_json(snapshot);
    body["repository"] = repository_json(*repository);
    body["issues"] = Json::array();
    body["pullRequests"] = Json::array();
    body["workflowRuns"] = Json::array();
    body["jobs"] = Json::array();
    for (const auto& issue : snapshot.issues) {
        if (issue.repository == full_name) {
            body["issues"].push_back(issue_json(issue));
        }
    }
    for (const auto& pull_request : snapshot.pull_requests) {
        if (pull_request.repository == full_name) {
            body["pullRequests"].push_back(pull_request_json(pull_request));
        }
    }
    for (const auto& run : snapshot.workflow_runs) {
        if (run.repository == full_name) {
            body["workflowRuns"].push_back(workflow_run_json(run));
        }
    }
    for (const auto& job : snapshot.jobs) {
        if (job.repository == full_name) {
            body["jobs"].push_back(workflow_job_json(job));
        }
    }
    return json_response(body);
}

JsonResponse make_issues_response(const Snapshot& snapshot,
                                  const std::optional<std::string>& repository) {
    Json issues = Json::array();
    for (const auto& issue : snapshot.issues) {
        if (matches_repository(repository, issue.repository)) {
            issues.push_back(issue_json(issue));
        }
    }
    auto body = base_json(snapshot);
    body["issues"] = std::move(issues);
    return json_response(body);
}

JsonResponse make_pull_requests_response(const Snapshot& snapshot,
                                         const std::optional<std::string>& repository) {
    Json pull_requests = Json::array();
    for (const auto& pull_request : snapshot.pull_requests) {
        if (matches_repository(repository, pull_request.repository)) {
            pull_requests.push_back(pull_request_json(pull_request));
        }
    }
    auto body = base_json(snapshot);
    body["pullRequests"] = std::move(pull_requests);
    return json_response(body);
}

JsonResponse make_workflow_runs_response(const Snapshot& snapshot,
                                         const std::optional<std::string>& repository,
                                         const std::optional<std::string>& status,
                                         const std::optional<std::string>& conclusion) {
    Json runs = Json::array();
    for (const auto& run : snapshot.workflow_runs) {
        if (matches_repository(repository, run.repository) &&
            (!status.has_value() || to_string(run.status) == *status) &&
            (!conclusion.has_value() ||
             (run.conclusion.has_value() && to_string(*run.conclusion) == *conclusion))) {
            runs.push_back(workflow_run_json(run));
        }
    }
    auto body = base_json(snapshot);
    body["workflowRuns"] = std::move(runs);
    return json_response(body);
}

JsonResponse make_workflow_jobs_response(const Snapshot& snapshot,
                                         const std::optional<std::string>& repository) {
    Json jobs = Json::array();
    for (const auto& job : snapshot.jobs) {
        if (matches_repository(repository, job.repository)) {
            jobs.push_back(workflow_job_json(job));
        }
    }
    auto body = base_json(snapshot);
    body["jobs"] = std::move(jobs);
    return json_response(body);
}

JsonResponse make_activity_response(const SnapshotStore& store) {
    const auto snapshot = store.get();
    if (snapshot == nullptr) {
        return make_snapshot_unavailable_response();
    }

    Json running_jobs = Json::array();
    Json failed_runs = Json::array();
    Json pull_requests = Json::array();
    Json issues = Json::array();
    for (const auto& job : snapshot->jobs) {
        if (job.status == RunStatus::queued || job.status == RunStatus::in_progress) {
            running_jobs.push_back(workflow_job_json(job));
        }
    }
    for (const auto& run : snapshot->workflow_runs) {
        if (run.conclusion == Conclusion::failure) {
            failed_runs.push_back(workflow_run_json(run));
        }
    }
    for (const auto& pull_request : snapshot->pull_requests) {
        pull_requests.push_back(pull_request_json(pull_request));
    }
    for (const auto& issue : snapshot->issues) {
        issues.push_back(issue_json(issue));
    }

    auto body = base_json(store, *snapshot);
    body["activity"] = Json{{"runningJobs", std::move(running_jobs)},
                            {"failedRuns", std::move(failed_runs)},
                            {"pullRequests", std::move(pull_requests)},
                            {"issues", std::move(issues)}};
    return json_response(body);
}

JsonResponse make_snapshot_unavailable_response() {
    return error_response("snapshot_unavailable", 503);
}

ApiServer::ApiServer(const Config& config, const SnapshotStore& store)
    : config_(config), store_(store) {
    register_routes();
}

void ApiServer::register_routes() {
    server_.Get("/healthz", [](const httplib::Request&, httplib::Response& response) {
        write_response(make_health_response(), response);
    });

    server_.Get("/readyz", [this](const httplib::Request&, httplib::Response& response) {
        write_response(make_readiness_response(store_.ready()), response);
    });

    server_.Get("/v1/meta", [this](const httplib::Request&, httplib::Response& response) {
        write_response(make_meta_response(store_), response);
    });

    server_.Get("/v1/summary", [this](const httplib::Request&, httplib::Response& response) {
        write_response(make_summary_response(store_), response);
    });

    server_.Get("/v1/repos", [this](const httplib::Request&, httplib::Response& response) {
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr ? make_snapshot_unavailable_response()
                                           : make_repositories_response(*snapshot),
                       response);
    });

    server_.Get(R"(/v1/repos/([^/]+)/([^/]+))", [this](const httplib::Request& request,
                                                       httplib::Response& response) {
        const auto full_name =
            std::string{request.matches[1]} + "/" + std::string{request.matches[2]};
        try {
            const auto parsed = parse_repository_ref(full_name);
            static_cast<void>(parsed);
        } catch (const std::exception&) {
            write_response(error_response("repository_not_found", 404), response);
            return;
        }
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr ? make_snapshot_unavailable_response()
                                           : make_repository_response(*snapshot, full_name),
                       response);
    });

    server_.Get("/v1/issues", [this](const httplib::Request& request, httplib::Response& response) {
        JsonResponse error;
        const auto repository = valid_repository_filter(request, error);
        if (!error.body.empty()) {
            write_response(error, response);
            return;
        }
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr ? make_snapshot_unavailable_response()
                                           : make_issues_response(*snapshot, repository),
                       response);
    });

    server_.Get("/v1/pulls", [this](const httplib::Request& request, httplib::Response& response) {
        JsonResponse error;
        const auto repository = valid_repository_filter(request, error);
        if (!error.body.empty()) {
            write_response(error, response);
            return;
        }
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr ? make_snapshot_unavailable_response()
                                           : make_pull_requests_response(*snapshot, repository),
                       response);
    });

    server_.Get("/v1/runs", [this](const httplib::Request& request, httplib::Response& response) {
        JsonResponse error;
        const auto repository = valid_repository_filter(request, error);
        if (!error.body.empty()) {
            write_response(error, response);
            return;
        }
        const auto status = query_param(request, "status");
        if (status.has_value() && !parse_run_status(*status).has_value()) {
            write_response(error_response("invalid_status_filter", 400), response);
            return;
        }
        const auto conclusion = query_param(request, "conclusion");
        if (conclusion.has_value() && !parse_conclusion(*conclusion).has_value()) {
            write_response(error_response("invalid_conclusion_filter", 400), response);
            return;
        }
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr
                           ? make_snapshot_unavailable_response()
                           : make_workflow_runs_response(*snapshot, repository, status, conclusion),
                       response);
    });

    server_.Get("/v1/jobs", [this](const httplib::Request& request, httplib::Response& response) {
        JsonResponse error;
        const auto repository = valid_repository_filter(request, error);
        if (!error.body.empty()) {
            write_response(error, response);
            return;
        }
        const auto snapshot = store_.get();
        write_response(snapshot == nullptr ? make_snapshot_unavailable_response()
                                           : make_workflow_jobs_response(*snapshot, repository),
                       response);
    });

    server_.Get("/v1/activity", [this](const httplib::Request&, httplib::Response& response) {
        write_response(make_activity_response(store_), response);
    });
}

bool ApiServer::listen() {
    return server_.listen(config_.bind_address, static_cast<int>(config_.port));
}

void ApiServer::stop() {
    server_.stop();
}

} // namespace ghinfo
