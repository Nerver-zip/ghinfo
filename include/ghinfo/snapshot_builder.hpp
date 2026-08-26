#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/snapshot.hpp"

#include <cstdint>
#include <string>

namespace ghinfo {

[[nodiscard]] Snapshot build_snapshot(const Config& config, const GitHubClient& github,
                                      std::uint64_t generation, std::string generated_at);

[[nodiscard]] std::string utc_now_iso8601();

} // namespace ghinfo
