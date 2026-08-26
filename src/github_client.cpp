#include "ghinfo/github_client.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

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

} // namespace ghinfo
