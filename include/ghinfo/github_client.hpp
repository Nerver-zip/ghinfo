#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ghinfo {

enum class GitHubErrorKind {
    transport,
    http,
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
    bool allow_not_modified{false};
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

    static constexpr std::string_view api_base{"https://api.github.com"};
    static constexpr std::string_view api_version{"2026-03-10"};
    static constexpr std::string_view user_agent{"ghinfo/0.1.0"};

  private:
    std::string token_;
    GitHubClientOptions options_;
};

} // namespace ghinfo
