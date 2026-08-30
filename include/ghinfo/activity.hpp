#pragma once

#include "ghinfo/snapshot.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace ghinfo {

inline constexpr std::size_t kDefaultActivityLimit = 20;
inline constexpr std::size_t kMaxActivityLimit = 100;

enum class ActivityCategory {
    workflows,
    pull_requests,
    issues,
};

[[nodiscard]] std::optional<ActivityCategory> parse_activity_category(std::string_view value);

[[nodiscard]] std::vector<ActivityItem> build_activity_items(const Snapshot& snapshot);

[[nodiscard]] std::vector<ActivityItem>
select_activity_items(const std::vector<ActivityItem>& items, std::size_t limit,
                      std::optional<ActivityCategory> category = std::nullopt);

} // namespace ghinfo
