#pragma once

#include "ghinfo/snapshot.hpp"

#include <cstddef>
#include <vector>

namespace ghinfo {

inline constexpr std::size_t kDefaultActivityLimit = 20;
inline constexpr std::size_t kMaxActivityLimit = 100;

[[nodiscard]] std::vector<ActivityItem> build_activity_items(const Snapshot& snapshot);

[[nodiscard]] std::vector<ActivityItem>
select_activity_items(const std::vector<ActivityItem>& items, std::size_t limit);

} // namespace ghinfo
