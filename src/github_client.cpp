#include "ghinfo/github_client.hpp"

#include <stdexcept>
#include <utility>

namespace ghinfo {

GitHubClient::GitHubClient(std::string token) : token_(std::move(token)) {
    if (token_.empty()) {
        throw std::invalid_argument("GitHub token must not be empty");
    }
}

const std::string& GitHubClient::token_for_transport_only() const noexcept {
    // This accessor exists only as a temporary seam for MVP-003 transport implementation.
    // It must never be used by API/logging code.
    return token_;
}

} // namespace ghinfo
