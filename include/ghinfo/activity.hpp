#pragma once

#include "ghinfo/snapshot.hpp"

#include <cstddef>
#include <vector>

namespace ghinfo {

inline constexpr std::size_t kDefaultActivityLimit = 20;
inline constexpr std::size_t kMaxActivityLimit = 100;

[[nodiscard]] std::vector<ActivityItem> build_activity_items(const Snapshot& snapshot);

} // namespace ghinfo
