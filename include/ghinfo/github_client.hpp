#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/model.hpp"

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ghinfo {

enum class GitHubErrorKind {
    transport,
    http,
    malformed_json,
    semantic,
};

class GitHubRequestError : public std::runtime_error {
  public:
    GitHubRequestError(GitHubErrorKind kind, std::optional<long> status_code, std::string message);

    [[nodiscard]] GitHubErrorKind kind() const noexcept;
    [[nodiscard]] std::optional<long> status_code() const noexcept;

  private:
    GitHubErrorKind kind_;
    std::optional<long> status_code_;
};

struct GitHubResponse {
    long status_code{};
    std::string body;
    std::map<std::string, std::string> headers;

    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;
};

struct GitHubRequestOptions {
    std::vector<std::string> extra_headers;
};

struct GitHubClientOptions {
    std::string base_url{"https://api.github.com"};
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds total_timeout{15000};
};

class GitHubClient {
  public:
    explicit GitHubClient(std::string token, GitHubClientOptions options = {});

    [[nodiscard]] GitHubResponse get(std::string_view path,
                                     const GitHubRequestOptions& options = {}) const;
    [[nodiscard]] std::vector<Issue> fetch_open_issues(const RepositoryRef& repository) const;
    [[nodiscard]] std::vector<PullRequest>
    fetch_open_pull_requests(const RepositoryRef& repository) const;
    [[nodiscard]] std::vector<WorkflowRun> fetch_workflow_runs(const RepositoryRef& repository,
                                                               std::uint32_t history_limit) const;
    [[nodiscard]] std::vector<WorkflowJob>
    fetch_relevant_workflow_jobs(const RepositoryRef& repository, const WorkflowRun& run) const;
    [[nodiscard]] Repository fetch_repository(const RepositoryRef& repository) const;
    [[nodiscard]] std::vector<RepositoryRef> fetch_accessible_repositories() const;
    [[nodiscard]] std::optional<RateLimit> rate_limit() const;
    [[nodiscard]] std::optional<std::uint64_t> retry_after_seconds() const;
    [[nodiscard]] std::optional<std::uint64_t> rate_limit_reset_epoch_seconds() const;

    static constexpr std::string_view api_base{"https://api.github.com"};
    static constexpr std::string_view api_version{"2026-03-10"};
    static constexpr std::string_view user_agent{"ghinfo/0.1.0"};

  private:
    struct CachedResponse {
        std::string etag;
        GitHubResponse response;
    };

    std::string token_;
    GitHubClientOptions options_;
    mutable std::mutex cache_mutex_;
    mutable std::map<std::string, CachedResponse> response_cache_;
    mutable std::mutex metadata_mutex_;
    mutable std::optional<RateLimit> rate_limit_;
    mutable std::optional<std::uint64_t> retry_after_seconds_;
    mutable std::optional<std::uint64_t> rate_limit_reset_epoch_seconds_;

    void update_rate_limit(const std::map<std::string, std::string>& headers) const;
};

} // namespace ghinfo
