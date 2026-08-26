#pragma once

#include "ghinfo/config.hpp"

#include <string>
#include <string_view>

namespace ghinfo {

class GitHubClient {
  public:
    explicit GitHubClient(std::string token);

    // MVP-003 implements authenticated GET transport.
    // Later milestones add conditional requests, pagination, and resource methods.
    [[nodiscard]] const std::string& token_for_transport_only() const noexcept;

    static constexpr std::string_view api_base{"https://api.github.com"};
    static constexpr std::string_view api_version{"2026-03-10"};
    static constexpr std::string_view user_agent{"ghinfo/0.1.0"};

  private:
    std::string token_;
};

} // namespace ghinfo
