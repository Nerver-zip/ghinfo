#include "ghinfo/github_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace ghinfo {
namespace {

struct CurlHandleDeleter {
    void operator()(CURL* handle) const noexcept {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }
};

struct CurlHeadersDeleter {
    void operator()(curl_slist* headers) const noexcept {
        if (headers != nullptr) {
            curl_slist_free_all(headers);
        }
    }
};

using CurlHandle = std::unique_ptr<CURL, CurlHandleDeleter>;
using CurlHeaders = std::unique_ptr<curl_slist, CurlHeadersDeleter>;
using Json = nlohmann::json;

class PayloadShapeError final : public std::runtime_error {
  public:
    explicit PayloadShapeError(std::string message) : std::runtime_error(std::move(message)) {}
};

void ensure_curl_initialized() {
    static std::once_flag once;
    static CURLcode initialization_result = CURLE_OK;
    std::call_once(once, [] { initialization_result = curl_global_init(CURL_GLOBAL_DEFAULT); });

    if (initialization_result != CURLE_OK) {
        throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                 "failed to initialize GitHub transport");
    }
}

std::size_t append_body(char* data, std::size_t size, std::size_t count, void* userdata) {
    auto& body = *static_cast<std::string*>(userdata);
    body.append(data, size * count);
    return size * count;
}

std::string trim(std::string value) {
    const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char character) {
                    return !is_space(static_cast<unsigned char>(character));
                }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](char character) {
                                 return !is_space(static_cast<unsigned char>(character));
                             })
                    .base(),
                value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contains_header(const std::vector<std::string>& headers, std::string_view name) {
    for (const auto& header : headers) {
        const auto separator = header.find(':');
        if (separator != std::string::npos &&
            lowercase(trim(header.substr(0, separator))) == name) {
            return true;
        }
    }
    return false;
}

std::size_t capture_header(char* data, std::size_t size, std::size_t count, void* userdata) {
    auto& headers = *static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line{data, size * count};
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
        return size * count;
    }

    auto name = lowercase(trim(line.substr(0, separator)));
    if (!name.empty()) {
        headers[std::move(name)] = trim(line.substr(separator + 1));
    }
    return size * count;
}

long timeout_milliseconds(std::chrono::milliseconds timeout) {
    const auto count = timeout.count();
    if (count <= 0) {
        return 1;
    }

    const auto maximum =
        static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<long>::max());
    return static_cast<long>(std::min(count, maximum));
}

void check_setopt(CURLcode result, std::string_view operation) {
    if (result != CURLE_OK) {
        throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                 "failed to configure GitHub request: " + std::string{operation});
    }
}

Json parse_json(std::string_view body) {
    try {
        return Json::parse(body);
    } catch (const Json::exception&) {
        throw GitHubRequestError(GitHubErrorKind::malformed_json, std::nullopt,
                                 "GitHub returned malformed JSON");
    }
}

template <typename T> T required_field(const Json& object, std::string_view name) {
    const auto key = std::string{name};
    if (!object.is_object() || !object.contains(key) || object.at(key).is_null()) {
        throw PayloadShapeError("missing required field " + key);
    }

    try {
        return object.at(key).get<T>();
    } catch (const Json::exception&) {
        throw PayloadShapeError("invalid field " + key);
    }
}

std::string optional_string(const Json& object, std::string_view name) {
    const auto key = std::string{name};
    if (!object.is_object() || !object.contains(key) || object.at(key).is_null()) {
        return {};
    }
    if (!object.at(key).is_string()) {
        throw PayloadShapeError("invalid field " + key);
    }
    return object.at(key).get<std::string>();
}

bool is_utc_timestamp(std::string_view value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != 'Z') {
        return false;
    }
    const auto is_digit = [](char character) { return character >= '0' && character <= '9'; };
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index != 4 && index != 7 && index != 10 && index != 13 && index != 16 && index != 19 &&
            !is_digit(value[index])) {
            return false;
        }
    }
    return true;
}

std::string required_utc_timestamp(const Json& object, std::string_view name) {
    const auto value = required_field<std::string>(object, name);
    if (!is_utc_timestamp(value)) {
        throw PayloadShapeError("invalid UTC timestamp " + std::string{name});
    }
    return value;
}

std::optional<std::string> nullable_string(const Json& object, std::string_view name) {
    const auto key = std::string{name};
    if (!object.is_object() || !object.contains(key) || object.at(key).is_null()) {
        return std::nullopt;
    }
    if (!object.at(key).is_string()) {
        throw PayloadShapeError("invalid field " + key);
    }
    const auto value = object.at(key).get<std::string>();
    if (!is_utc_timestamp(value)) {
        throw PayloadShapeError("invalid UTC timestamp " + key);
    }
    return value;
}

std::string author_name(const Json& object) {
    if (!object.is_object() || !object.contains("user") || object.at("user").is_null()) {
        return "unknown";
    }
    const auto login = optional_string(object.at("user"), "login");
    if (login.empty()) {
        throw PayloadShapeError("invalid user field");
    }
    return login;
}

bool has_next_link(const GitHubResponse& response) {
    const auto link = response.header("link");
    if (!link.has_value()) {
        return false;
    }
    return link->find("rel=\"next\"") != std::string::npos ||
           link->find("rel=next") != std::string::npos;
}

std::vector<Issue> parse_issue_page(const Json& payload, const RepositoryRef& repository) {
    if (!payload.is_array()) {
        throw PayloadShapeError("issues payload must be an array");
    }

    std::vector<Issue> issues;
    issues.reserve(payload.size());
    for (const auto& entry : payload) {
        if (!entry.is_object()) {
            throw PayloadShapeError("issue entry must be an object");
        }
        if (entry.contains("pull_request")) {
            continue;
        }

        Issue issue;
        issue.id = required_field<std::uint64_t>(entry, "id");
        issue.number = required_field<std::uint64_t>(entry, "number");
        issue.repository = repository.full_name();
        issue.title = required_field<std::string>(entry, "title");
        issue.author = author_name(entry);
        issue.created_at = required_utc_timestamp(entry, "created_at");
        issue.updated_at = required_utc_timestamp(entry, "updated_at");
        issue.url = required_field<std::string>(entry, "html_url");

        const auto& labels = entry.at("labels");
        if (!labels.is_array()) {
            throw PayloadShapeError("issue labels must be an array");
        }
        issue.labels.reserve(labels.size());
        for (const auto& label : labels) {
            issue.labels.push_back(required_field<std::string>(label, "name"));
        }
        issues.push_back(std::move(issue));
    }
    return issues;
}

std::string required_nested_string(const Json& object, std::string_view parent,
                                   std::string_view child) {
    const auto parent_key = std::string{parent};
    if (!object.is_object() || !object.contains(parent_key) || !object.at(parent_key).is_object()) {
        throw PayloadShapeError("missing required object " + parent_key);
    }
    return required_field<std::string>(object.at(parent_key), child);
}

std::vector<PullRequest> parse_pull_request_page(const Json& payload,
                                                 const RepositoryRef& repository) {
    if (!payload.is_array()) {
        throw PayloadShapeError("pull requests payload must be an array");
    }

    std::vector<PullRequest> pull_requests;
    pull_requests.reserve(payload.size());
    for (const auto& entry : payload) {
        if (!entry.is_object()) {
            throw PayloadShapeError("pull request entry must be an object");
        }

        PullRequest pull_request;
        pull_request.id = required_field<std::uint64_t>(entry, "id");
        pull_request.number = required_field<std::uint64_t>(entry, "number");
        pull_request.repository = repository.full_name();
        pull_request.title = required_field<std::string>(entry, "title");
        pull_request.author = author_name(entry);
        pull_request.draft = required_field<bool>(entry, "draft");
        pull_request.head = required_nested_string(entry, "head", "ref");
        pull_request.base = required_nested_string(entry, "base", "ref");
        pull_request.created_at = required_utc_timestamp(entry, "created_at");
        pull_request.updated_at = required_utc_timestamp(entry, "updated_at");
        pull_request.url = required_field<std::string>(entry, "html_url");
        pull_requests.push_back(std::move(pull_request));
    }
    return pull_requests;
}

RunStatus parse_run_status(std::string_view value) {
    if (value == "queued") {
        return RunStatus::queued;
    }
    if (value == "in_progress") {
        return RunStatus::in_progress;
    }
    if (value == "completed") {
        return RunStatus::completed;
    }
    return RunStatus::unknown;
}

Conclusion parse_conclusion(std::string_view value) {
    if (value == "success") {
        return Conclusion::success;
    }
    if (value == "failure") {
        return Conclusion::failure;
    }
    if (value == "cancelled") {
        return Conclusion::cancelled;
    }
    if (value == "skipped") {
        return Conclusion::skipped;
    }
    if (value == "timed_out") {
        return Conclusion::timed_out;
    }
    if (value == "neutral") {
        return Conclusion::neutral;
    }
    if (value == "action_required") {
        return Conclusion::action_required;
    }
    return Conclusion::unknown;
}

std::optional<Conclusion> optional_conclusion(const Json& object) {
    if (!object.is_object() || !object.contains("conclusion") ||
        object.at("conclusion").is_null()) {
        return std::nullopt;
    }
    return parse_conclusion(required_field<std::string>(object, "conclusion"));
}

std::vector<WorkflowRun> parse_workflow_run_page(const Json& payload,
                                                 const RepositoryRef& repository) {
    if (!payload.is_object() || !payload.contains("workflow_runs") ||
        !payload.at("workflow_runs").is_array()) {
        throw PayloadShapeError("workflow runs payload must contain an array");
    }

    const auto& entries = payload.at("workflow_runs");
    std::vector<WorkflowRun> runs;
    runs.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!entry.is_object()) {
            throw PayloadShapeError("workflow run entry must be an object");
        }

        WorkflowRun run;
        run.id = required_field<std::uint64_t>(entry, "id");
        run.repository = repository.full_name();
        run.name = required_field<std::string>(entry, "name");
        run.status = parse_run_status(required_field<std::string>(entry, "status"));
        run.conclusion = optional_conclusion(entry);
        run.branch = optional_string(entry, "head_branch");
        run.commit_sha = required_field<std::string>(entry, "head_sha");
        run.event = required_field<std::string>(entry, "event");
        run.created_at = required_utc_timestamp(entry, "created_at");
        run.updated_at = required_utc_timestamp(entry, "updated_at");
        run.url = required_field<std::string>(entry, "html_url");
        runs.push_back(std::move(run));
    }
    return runs;
}

std::vector<WorkflowJob> parse_workflow_job_page(const Json& payload,
                                                 const RepositoryRef& repository) {
    if (!payload.is_object() || !payload.contains("jobs") || !payload.at("jobs").is_array()) {
        throw PayloadShapeError("workflow jobs payload must contain an array");
    }

    const auto& entries = payload.at("jobs");
    std::vector<WorkflowJob> jobs;
    jobs.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!entry.is_object()) {
            throw PayloadShapeError("workflow job entry must be an object");
        }

        WorkflowJob job;
        job.id = required_field<std::uint64_t>(entry, "id");
        job.run_id = required_field<std::uint64_t>(entry, "run_id");
        job.repository = repository.full_name();
        job.name = required_field<std::string>(entry, "name");
        job.status = parse_run_status(required_field<std::string>(entry, "status"));
        job.conclusion = optional_conclusion(entry);
        job.started_at = nullable_string(entry, "started_at");
        job.completed_at = nullable_string(entry, "completed_at");
        job.url = required_field<std::string>(entry, "html_url");
        jobs.push_back(std::move(job));
    }
    return jobs;
}

bool needs_job_details(const WorkflowRun& run) {
    if (run.status != RunStatus::completed || !run.conclusion.has_value()) {
        return true;
    }
    return run.conclusion.value() != Conclusion::success &&
           run.conclusion.value() != Conclusion::skipped &&
           run.conclusion.value() != Conclusion::neutral;
}

std::optional<std::uint32_t> header_uint32(const std::map<std::string, std::string>& headers,
                                           std::string_view name) {
    const auto found = headers.find(std::string{name});
    if (found == headers.end()) {
        return std::nullopt;
    }

    std::uint32_t value{};
    const auto [end, error] =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (error != std::errc{} || end != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint64_t> header_uint64(const std::map<std::string, std::string>& headers,
                                           std::string_view name) {
    const auto found = headers.find(std::string{name});
    if (found == headers.end()) {
        return std::nullopt;
    }

    std::uint64_t value{};
    const auto [end, error] =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (error != std::errc{} || end != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> format_reset_time(const std::map<std::string, std::string>& headers) {
    const auto found = headers.find("x-ratelimit-reset");
    if (found == headers.end()) {
        return std::nullopt;
    }

    const auto epoch_seconds = header_uint64(headers, "x-ratelimit-reset");
    if (!epoch_seconds.has_value() ||
        epoch_seconds.value() >
            static_cast<std::uint64_t>(std::numeric_limits<std::time_t>::max())) {
        return std::nullopt;
    }

    const auto epoch = static_cast<std::time_t>(epoch_seconds.value());
    std::tm utc{};
    if (gmtime_r(&epoch, &utc) == nullptr) {
        return std::nullopt;
    }

    char buffer[32]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        return std::nullopt;
    }
    return std::string{buffer};
}

Repository parse_repository_payload(const Json& payload, const RepositoryRef& requested) {
    if (!payload.is_object()) {
        throw PayloadShapeError("repository payload must be an object");
    }

    Repository repository;
    repository.id = required_field<std::uint64_t>(payload, "id");
    repository.full_name = required_field<std::string>(payload, "full_name");
    if (repository.full_name != requested.full_name()) {
        throw PayloadShapeError("repository full_name does not match request");
    }
    repository.is_private = required_field<bool>(payload, "private");
    repository.default_branch = required_field<std::string>(payload, "default_branch");
    repository.url = required_field<std::string>(payload, "html_url");
    repository.updated_at = required_utc_timestamp(payload, "updated_at");
    return repository;
}

std::vector<RepositoryRef> parse_repository_list_payload(const Json& payload) {
    if (!payload.is_array()) {
        throw PayloadShapeError("repository list payload must be an array");
    }

    std::vector<RepositoryRef> repositories;
    repositories.reserve(payload.size());
    for (const auto& entry : payload) {
        const auto full_name = required_field<std::string>(entry, "full_name");
        RepositoryRef repository;
        try {
            repository = parse_repository_ref(full_name);
        } catch (const std::exception&) {
            throw PayloadShapeError("invalid repository full_name");
        }
        repositories.push_back(std::move(repository));
    }
    return repositories;
}

} // namespace

GitHubRequestError::GitHubRequestError(GitHubErrorKind kind, std::optional<long> status_code,
                                       std::string message)
    : std::runtime_error(std::move(message)), kind_(kind), status_code_(status_code) {}

GitHubErrorKind GitHubRequestError::kind() const noexcept {
    return kind_;
}

std::optional<long> GitHubRequestError::status_code() const noexcept {
    return status_code_;
}

std::optional<std::string> GitHubResponse::header(std::string_view name) const {
    auto normalized = std::string{name};
    std::ranges::transform(normalized, normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    const auto found = headers.find(normalized);
    if (found == headers.end()) {
        return std::nullopt;
    }
    return found->second;
}

GitHubClient::GitHubClient(std::string token, GitHubClientOptions options)
    : token_(std::move(token)), options_(std::move(options)) {
    if (token_.empty()) {
        throw std::invalid_argument("GitHub token must not be empty");
    }
    if (options_.base_url.empty()) {
        throw std::invalid_argument("GitHub base URL must not be empty");
    }
    while (options_.base_url.size() > 1 && options_.base_url.back() == '/') {
        options_.base_url.pop_back();
    }
    ensure_curl_initialized();
}

GitHubResponse GitHubClient::get(std::string_view path,
                                 const GitHubRequestOptions& request_options) const {
    if (path.empty() || path.front() != '/' ||
        path.find_first_of("\r\n") != std::string_view::npos) {
        throw std::invalid_argument("GitHub request path must be an absolute API path");
    }

    ensure_curl_initialized();

    const std::string cache_key{path};
    std::optional<CachedResponse> cached;
    {
        std::lock_guard lock{cache_mutex_};
        const auto found = response_cache_.find(cache_key);
        if (found != response_cache_.end()) {
            cached = found->second;
        }
    }

    CurlHandle handle{curl_easy_init()};
    if (!handle) {
        throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                 "failed to create GitHub request handle");
    }

    std::string body;
    std::map<std::string, std::string> response_headers;
    const std::string url = options_.base_url + std::string{path};

    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str()), "URL");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_HTTPGET, 1L), "method");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT_MS,
                                  timeout_milliseconds(options_.connect_timeout)),
                 "connect timeout");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT_MS,
                                  timeout_milliseconds(options_.total_timeout)),
                 "total timeout");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L), "signal handling");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &append_body),
                 "body callback");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &body), "body callback data");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_HEADERFUNCTION, &capture_header),
                 "header callback");
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_HEADERDATA, &response_headers),
                 "header callback data");

    CurlHeaders headers;
    const auto add_header = [&](std::string value) {
        auto* updated = curl_slist_append(headers.get(), value.c_str());
        if (updated == nullptr) {
            throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                     "failed to allocate GitHub request headers");
        }
        headers.release();
        headers.reset(updated);
    };

    add_header("Accept: application/vnd.github+json");
    add_header("X-GitHub-Api-Version: " + std::string{api_version});
    add_header("User-Agent: " + std::string{user_agent});
    add_header("Authorization: Bearer " + token_);
    if (cached.has_value() && !contains_header(request_options.extra_headers, "if-none-match")) {
        add_header("If-None-Match: " + cached->etag);
    }
    for (const auto& extra_header : request_options.extra_headers) {
        if (extra_header.find_first_of("\r\n") != std::string::npos) {
            throw std::invalid_argument("GitHub request header contains a line break");
        }
        add_header(extra_header);
    }
    check_setopt(curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get()),
                 "request headers");

    const auto result = curl_easy_perform(handle.get());
    if (result != CURLE_OK) {
        throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                 "GitHub request failed: " +
                                     std::string{curl_easy_strerror(result)});
    }

    long status_code = 0;
    const auto info_result = curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status_code);
    if (info_result != CURLE_OK) {
        throw GitHubRequestError(GitHubErrorKind::transport, std::nullopt,
                                 "failed to read GitHub response status");
    }
    update_rate_limit(response_headers);

    const bool successful = status_code >= 200 && status_code < 300;
    const bool not_modified = status_code == 304;
    if (!successful && !not_modified) {
        throw GitHubRequestError(GitHubErrorKind::http, status_code,
                                 "GitHub returned HTTP " + std::to_string(status_code) + " for " +
                                     std::string{path});
    }

    if (not_modified) {
        if (!cached.has_value()) {
            throw GitHubRequestError(GitHubErrorKind::http, status_code,
                                     "GitHub returned HTTP 304 without a cached response for " +
                                         std::string{path});
        }

        auto merged_headers = cached->response.headers;
        for (auto& [name, value] : response_headers) {
            merged_headers[name] = std::move(value);
        }
        return GitHubResponse{status_code, cached->response.body, std::move(merged_headers)};
    }

    GitHubResponse response{status_code, std::move(body), std::move(response_headers)};
    const auto etag = response.header("etag");
    {
        std::lock_guard lock{cache_mutex_};
        if (etag.has_value() && !etag->empty()) {
            response_cache_[cache_key] = CachedResponse{*etag, response};
        } else {
            response_cache_.erase(cache_key);
        }
    }
    return response;
}

std::vector<Issue> GitHubClient::fetch_open_issues(const RepositoryRef& repository) const {
    constexpr std::size_t page_size = 100;
    constexpr std::size_t max_pages = 1000;
    std::vector<Issue> issues;

    for (std::size_t page = 1; page <= max_pages; ++page) {
        const auto path = "/repos/" + repository.full_name() +
                          "/issues?state=open&per_page=" + std::to_string(page_size) +
                          "&page=" + std::to_string(page);
        const auto response = get(path);
        const auto payload = parse_json(response.body);

        try {
            auto page_issues = parse_issue_page(payload, repository);
            issues.insert(issues.end(), std::make_move_iterator(page_issues.begin()),
                          std::make_move_iterator(page_issues.end()));
        } catch (const PayloadShapeError& error) {
            throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                     "GitHub issues payload has invalid shape: " +
                                         std::string{error.what()});
        }

        if (!has_next_link(response)) {
            return issues;
        }
    }

    throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                             "GitHub issues pagination exceeded the safety limit");
}

std::vector<PullRequest>
GitHubClient::fetch_open_pull_requests(const RepositoryRef& repository) const {
    constexpr std::size_t page_size = 100;
    constexpr std::size_t max_pages = 1000;
    std::vector<PullRequest> pull_requests;

    for (std::size_t page = 1; page <= max_pages; ++page) {
        const auto path = "/repos/" + repository.full_name() +
                          "/pulls?state=open&per_page=" + std::to_string(page_size) +
                          "&page=" + std::to_string(page);
        const auto response = get(path);
        const auto payload = parse_json(response.body);

        try {
            auto page_pull_requests = parse_pull_request_page(payload, repository);
            pull_requests.insert(pull_requests.end(),
                                 std::make_move_iterator(page_pull_requests.begin()),
                                 std::make_move_iterator(page_pull_requests.end()));
        } catch (const PayloadShapeError& error) {
            throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                     "GitHub pull requests payload has invalid shape: " +
                                         std::string{error.what()});
        }

        if (!has_next_link(response)) {
            return pull_requests;
        }
    }

    throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                             "GitHub pull requests pagination exceeded the safety limit");
}

std::vector<WorkflowRun> GitHubClient::fetch_workflow_runs(const RepositoryRef& repository,
                                                           std::uint32_t history_limit) const {
    if (history_limit == 0 || history_limit > 100) {
        throw std::invalid_argument("workflow run history must be between 1 and 100");
    }

    const auto path = "/repos/" + repository.full_name() +
                      "/actions/runs?per_page=" + std::to_string(history_limit) + "&page=1";
    const auto response = get(path);
    const auto payload = parse_json(response.body);

    try {
        auto runs = parse_workflow_run_page(payload, repository);
        if (runs.size() > history_limit) {
            runs.resize(history_limit);
        }
        return runs;
    } catch (const PayloadShapeError& error) {
        throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                 "GitHub workflow runs payload has invalid shape: " +
                                     std::string{error.what()});
    }
}

std::vector<WorkflowJob> GitHubClient::fetch_relevant_workflow_jobs(const RepositoryRef& repository,
                                                                    const WorkflowRun& run) const {
    if (!needs_job_details(run)) {
        return {};
    }

    constexpr std::size_t page_size = 100;
    constexpr std::size_t max_pages = 1000;
    std::vector<WorkflowJob> jobs;

    for (std::size_t page = 1; page <= max_pages; ++page) {
        const auto path = "/repos/" + repository.full_name() + "/actions/runs/" +
                          std::to_string(run.id) + "/jobs?per_page=" + std::to_string(page_size) +
                          "&page=" + std::to_string(page);
        const auto response = get(path);
        const auto payload = parse_json(response.body);

        try {
            auto page_jobs = parse_workflow_job_page(payload, repository);
            jobs.insert(jobs.end(), std::make_move_iterator(page_jobs.begin()),
                        std::make_move_iterator(page_jobs.end()));
        } catch (const PayloadShapeError& error) {
            throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                     "GitHub workflow jobs payload has invalid shape: " +
                                         std::string{error.what()});
        }

        if (!has_next_link(response)) {
            return jobs;
        }
    }

    throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                             "GitHub workflow jobs pagination exceeded the safety limit");
}

Repository GitHubClient::fetch_repository(const RepositoryRef& repository) const {
    const auto path = "/repos/" + repository.full_name();
    const auto response = get(path);
    const auto payload = parse_json(response.body);

    try {
        return parse_repository_payload(payload, repository);
    } catch (const PayloadShapeError& error) {
        throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                 "GitHub repository payload has invalid shape: " +
                                     std::string{error.what()});
    }
}

std::vector<RepositoryRef> GitHubClient::fetch_accessible_repositories() const {
    constexpr std::size_t page_size = 100;
    constexpr std::size_t max_pages = 1000;
    std::vector<RepositoryRef> repositories;
    std::set<std::string> seen;

    for (std::size_t page = 1; page <= max_pages; ++page) {
        const auto path =
            "/user/repos?visibility=all&affiliation=owner,collaborator,organization_member"
            "&sort=full_name&direction=asc&per_page=" +
            std::to_string(page_size) + "&page=" + std::to_string(page);
        const auto response = get(path);
        const auto payload = parse_json(response.body);

        try {
            auto page_repositories = parse_repository_list_payload(payload);
            for (auto& repository : page_repositories) {
                if (!seen.insert(repository.full_name()).second) {
                    throw PayloadShapeError("duplicate repository full_name");
                }
                repositories.push_back(std::move(repository));
            }
        } catch (const PayloadShapeError& error) {
            throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                                     "GitHub repository list payload has invalid shape: " +
                                         std::string{error.what()});
        }

        if (!has_next_link(response)) {
            return repositories;
        }
    }

    throw GitHubRequestError(GitHubErrorKind::semantic, std::nullopt,
                             "GitHub repository pagination exceeded the safety limit");
}

std::optional<RateLimit> GitHubClient::rate_limit() const {
    std::lock_guard lock{metadata_mutex_};
    return rate_limit_;
}

std::optional<std::uint64_t> GitHubClient::retry_after_seconds() const {
    std::lock_guard lock{metadata_mutex_};
    return retry_after_seconds_;
}

std::optional<std::uint64_t> GitHubClient::rate_limit_reset_epoch_seconds() const {
    std::lock_guard lock{metadata_mutex_};
    return rate_limit_reset_epoch_seconds_;
}

void GitHubClient::update_rate_limit(const std::map<std::string, std::string>& headers) const {
    const auto retry_after = header_uint64(headers, "retry-after");
    const auto reset_epoch = header_uint64(headers, "x-ratelimit-reset");
    const auto limit = header_uint32(headers, "x-ratelimit-limit");
    const auto remaining = header_uint32(headers, "x-ratelimit-remaining");
    {
        std::lock_guard lock{metadata_mutex_};
        retry_after_seconds_ = retry_after;
        rate_limit_reset_epoch_seconds_ = reset_epoch;
    }
    if (!limit.has_value() || !remaining.has_value()) {
        return;
    }

    RateLimit rate_limit{*limit, *remaining, format_reset_time(headers)};
    std::lock_guard lock{metadata_mutex_};
    rate_limit_ = std::move(rate_limit);
}

} // namespace ghinfo
