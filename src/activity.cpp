#include "ghinfo/activity.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>
#include <tuple>
#include <utility>

namespace ghinfo {
namespace {

using Timestamp = std::chrono::sys_seconds;

constexpr auto kRecentFailureAge = std::chrono::hours{24 * 7};
constexpr auto kMaximumFailureAge = std::chrono::hours{24 * 30};

enum class FailureAge {
    recent,
    stale,
    expired,
    unknown,
};

[[nodiscard]] std::optional<int> fixed_number(std::string_view value, std::size_t offset,
                                              std::size_t length) {
    if (offset + length > value.size()) {
        return std::nullopt;
    }

    int number = 0;
    for (std::size_t index = offset; index < offset + length; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return std::nullopt;
        }
        number = number * 10 + (value[index] - '0');
    }
    return number;
}

[[nodiscard]] std::optional<Timestamp> parse_timestamp(std::string_view value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != 'Z') {
        return std::nullopt;
    }

    const auto year = fixed_number(value, 0, 4);
    const auto month = fixed_number(value, 5, 2);
    const auto day = fixed_number(value, 8, 2);
    const auto hour = fixed_number(value, 11, 2);
    const auto minute = fixed_number(value, 14, 2);
    const auto second = fixed_number(value, 17, 2);
    if (!year.has_value() || !month.has_value() || !day.has_value() || !hour.has_value() ||
        !minute.has_value() || !second.has_value()) {
        return std::nullopt;
    }

    const std::chrono::year_month_day date{std::chrono::year{*year},
                                           std::chrono::month{static_cast<unsigned>(*month)},
                                           std::chrono::day{static_cast<unsigned>(*day)}};
    if (!date.ok() || *hour > 23 || *minute > 59 || *second > 59) {
        return std::nullopt;
    }

    const auto day_point = std::chrono::sys_days{date};
    return Timestamp{day_point.time_since_epoch() + std::chrono::hours{*hour} +
                     std::chrono::minutes{*minute} + std::chrono::seconds{*second}};
}

[[nodiscard]] FailureAge classify_failure_age(const std::optional<std::string>& occurred_at,
                                              std::string_view generated_at) {
    if (!occurred_at.has_value()) {
        return FailureAge::unknown;
    }

    const auto occurred = parse_timestamp(*occurred_at);
    const auto reference = parse_timestamp(generated_at);
    if (!occurred.has_value() || !reference.has_value()) {
        return FailureAge::unknown;
    }

    const auto age = *reference > *occurred ? *reference - *occurred : Timestamp::duration::zero();
    if (age <= kRecentFailureAge) {
        return FailureAge::recent;
    }
    if (age <= kMaximumFailureAge) {
        return FailureAge::stale;
    }
    return FailureAge::expired;
}

[[nodiscard]] ActivityItem base_item(ActivityKind kind, ActivityPriority priority,
                                     std::string signal, std::string repository, GithubId id,
                                     std::optional<std::string> updated_at, std::string url) {
    ActivityItem item;
    item.kind = kind;
    item.priority = priority;
    item.signals.push_back(std::move(signal));
    item.repository = std::move(repository);
    item.id = id;
    item.updated_at = std::move(updated_at);
    item.url = std::move(url);
    return item;
}

[[nodiscard]] int priority_rank(ActivityPriority priority) {
    switch (priority) {
    case ActivityPriority::critical:
        return 0;
    case ActivityPriority::high:
        return 1;
    case ActivityPriority::normal:
        return 2;
    }
    return 2;
}

[[nodiscard]] std::optional<std::string> job_timestamp(const WorkflowJob& job) {
    if (job.completed_at.has_value()) {
        return job.completed_at;
    }
    return job.started_at;
}

[[nodiscard]] bool is_active(const WorkflowJob& job) {
    return job.status == RunStatus::queued || job.status == RunStatus::in_progress;
}

[[nodiscard]] bool is_active(const WorkflowRun& run) {
    return run.status == RunStatus::queued || run.status == RunStatus::in_progress;
}

[[nodiscard]] bool activity_less(const ActivityItem& left, const ActivityItem& right) {
    if (priority_rank(left.priority) != priority_rank(right.priority)) {
        return priority_rank(left.priority) < priority_rank(right.priority);
    }

    if (left.updated_at.has_value() != right.updated_at.has_value()) {
        return left.updated_at.has_value();
    }
    if (left.updated_at.has_value() && left.updated_at != right.updated_at) {
        return *left.updated_at > *right.updated_at;
    }

    return std::tuple{left.repository, to_string(left.kind), left.id} <
           std::tuple{right.repository, to_string(right.kind), right.id};
}

[[nodiscard]] int category(ActivityKind kind) {
    switch (kind) {
    case ActivityKind::failed_job:
    case ActivityKind::failed_run:
    case ActivityKind::running_job:
    case ActivityKind::running_run:
        return 0;
    case ActivityKind::pull_request:
        return 1;
    case ActivityKind::issue:
        return 2;
    }
    return 2;
}

[[nodiscard]] bool belongs_to(ActivityKind kind, ActivityCategory activity_category) {
    switch (activity_category) {
    case ActivityCategory::workflows:
        return kind == ActivityKind::failed_job || kind == ActivityKind::failed_run ||
               kind == ActivityKind::running_job || kind == ActivityKind::running_run;
    case ActivityCategory::pull_requests:
        return kind == ActivityKind::pull_request;
    case ActivityCategory::issues:
        return kind == ActivityKind::issue;
    }
    return false;
}

[[nodiscard]] bool same_item(const ActivityItem& left, const ActivityItem& right) {
    return left.kind == right.kind && left.repository == right.repository && left.id == right.id;
}

[[nodiscard]] std::optional<GithubId> failed_incident_id(const ActivityItem& item) {
    if (item.kind == ActivityKind::failed_run) {
        return item.id;
    }
    if (item.kind == ActivityKind::failed_job && item.run_id.has_value()) {
        return item.run_id;
    }
    return std::nullopt;
}

[[nodiscard]] bool same_failed_incident(const ActivityItem& left, const ActivityItem& right) {
    const auto left_id = failed_incident_id(left);
    const auto right_id = failed_incident_id(right);
    return left_id.has_value() && right_id.has_value() && left.repository == right.repository &&
           *left_id == *right_id;
}

[[nodiscard]] bool selected_contains(const std::vector<ActivityItem>& selected,
                                     const ActivityItem& candidate) {
    return std::any_of(selected.begin(), selected.end(), [&candidate](const ActivityItem& item) {
        return same_item(item, candidate);
    });
}

[[nodiscard]] bool conflicts_with_first_three(const std::vector<ActivityItem>& selected,
                                              std::size_t replaced_index,
                                              const ActivityItem& candidate) {
    const auto protected_count = std::min<std::size_t>(3, selected.size());
    for (std::size_t index = 0; index < protected_count; ++index) {
        if (index != replaced_index && same_failed_incident(selected[index], candidate)) {
            return true;
        }
    }
    return false;
}

void remove_top_three_incident_duplicates(const std::vector<ActivityItem>& all_items,
                                          std::vector<ActivityItem>& selected) {
    bool needs_sort = true;
    for (;;) {
        if (needs_sort) {
            std::sort(selected.begin(), selected.end(), activity_less);
            needs_sort = false;
        }
        const auto protected_count = std::min<std::size_t>(3, selected.size());
        bool handled = false;

        for (std::size_t index = 0; index < protected_count && !handled; ++index) {
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (!same_failed_incident(selected[index], selected[previous])) {
                    continue;
                }

                if (selected.size() > protected_count) {
                    const auto alternative = std::find_if(
                        selected.begin() + static_cast<std::ptrdiff_t>(protected_count),
                        selected.end(), [&](const ActivityItem& candidate) {
                            return !conflicts_with_first_three(selected, index, candidate);
                        });
                    if (alternative != selected.end()) {
                        std::rotate(selected.begin() + static_cast<std::ptrdiff_t>(index),
                                    selected.begin() + static_cast<std::ptrdiff_t>(index + 1),
                                    alternative + 1);
                        handled = true;
                        break;
                    }
                }

                const auto replacement = std::find_if(
                    all_items.begin(), all_items.end(), [&](const ActivityItem& candidate) {
                        return !selected_contains(selected, candidate) &&
                               !conflicts_with_first_three(selected, index, candidate);
                    });
                if (replacement != all_items.end()) {
                    selected[index] = *replacement;
                    handled = true;
                    needs_sort = true;
                }
                break;
            }
        }
        if (!handled) {
            return;
        }
    }
}

} // namespace

std::optional<ActivityCategory> parse_activity_category(std::string_view value) {
    if (value == "workflows") {
        return ActivityCategory::workflows;
    }
    if (value == "pull_requests") {
        return ActivityCategory::pull_requests;
    }
    if (value == "issues") {
        return ActivityCategory::issues;
    }
    return std::nullopt;
}

std::vector<ActivityItem> build_activity_items(const Snapshot& snapshot) {
    std::vector<ActivityItem> items;
    items.reserve(snapshot.jobs.size() + snapshot.workflow_runs.size() +
                  snapshot.pull_requests.size() + snapshot.recent_closed_pull_requests.size() +
                  snapshot.issues.size());

    for (const auto& job : snapshot.jobs) {
        if (is_active(job)) {
            auto item =
                base_item(ActivityKind::running_job, ActivityPriority::critical, "running_job",
                          job.repository, job.id, job_timestamp(job), job.url);
            item.run_id = job.run_id;
            item.name = job.name;
            item.status = job.status;
            items.push_back(std::move(item));
        } else if (job.conclusion == Conclusion::failure) {
            auto item = base_item(ActivityKind::failed_job, ActivityPriority::high, "failed_job",
                                  job.repository, job.id, job_timestamp(job), job.url);
            const auto failure_age = classify_failure_age(item.updated_at, snapshot.generated_at);
            if (failure_age == FailureAge::expired) {
                continue;
            }
            if (failure_age == FailureAge::stale) {
                item.priority = ActivityPriority::normal;
                item.signals.push_back("stale_failure");
            } else if (failure_age == FailureAge::recent) {
                item.signals.push_back("recent_failure");
            }
            item.run_id = job.run_id;
            item.name = job.name;
            item.status = job.status;
            item.conclusion = job.conclusion;
            items.push_back(std::move(item));
        }
    }

    for (const auto& run : snapshot.workflow_runs) {
        if (is_active(run)) {
            auto item = base_item(ActivityKind::running_run, ActivityPriority::critical,
                                  "running_run", run.repository, run.id, run.updated_at, run.url);
            item.name = run.name;
            item.status = run.status;
            item.conclusion = run.conclusion;
            items.push_back(std::move(item));
        } else if (run.conclusion == Conclusion::failure) {
            auto item = base_item(ActivityKind::failed_run, ActivityPriority::high, "failed_run",
                                  run.repository, run.id, run.updated_at, run.url);
            const auto failure_age = classify_failure_age(item.updated_at, snapshot.generated_at);
            if (failure_age == FailureAge::expired) {
                continue;
            }
            if (failure_age == FailureAge::stale) {
                item.priority = ActivityPriority::normal;
                item.signals.push_back("stale_failure");
            } else if (failure_age == FailureAge::recent) {
                item.signals.push_back("recent_failure");
            }
            item.name = run.name;
            item.status = run.status;
            item.conclusion = run.conclusion;
            items.push_back(std::move(item));
        }
    }

    for (const auto& pull_request : snapshot.pull_requests) {
        auto item = base_item(ActivityKind::pull_request, ActivityPriority::high,
                              "open_pull_request", pull_request.repository, pull_request.id,
                              pull_request.updated_at, pull_request.url);
        item.number = pull_request.number;
        item.title = pull_request.title;
        items.push_back(std::move(item));
    }

    for (const auto& pull_request : snapshot.recent_closed_pull_requests) {
        auto item = base_item(ActivityKind::pull_request, ActivityPriority::normal,
                              "recent_closed_pull_request", pull_request.repository,
                              pull_request.id, pull_request.updated_at, pull_request.url);
        item.number = pull_request.number;
        item.title = pull_request.title;
        items.push_back(std::move(item));
    }

    for (const auto& issue : snapshot.issues) {
        auto item = base_item(ActivityKind::issue, ActivityPriority::normal, "open_issue",
                              issue.repository, issue.id, issue.updated_at, issue.url);
        item.number = issue.number;
        item.title = issue.title;
        items.push_back(std::move(item));
    }

    std::sort(items.begin(), items.end(), activity_less);
    return items;
}

std::vector<ActivityItem> select_activity_items(const std::vector<ActivityItem>& items,
                                                std::size_t limit,
                                                std::optional<ActivityCategory> category_filter) {
    if (limit == 0 || items.empty()) {
        return {};
    }

    std::vector<ActivityItem> selected;
    selected.reserve(std::min(limit, items.size()));

    if (category_filter.has_value()) {
        std::vector<ActivityItem> category_items;
        category_items.reserve(items.size());
        for (const auto& item : items) {
            if (belongs_to(item.kind, *category_filter)) {
                category_items.push_back(item);
            }
        }
        selected.reserve(std::min(limit, category_items.size()));
        for (const auto& item : category_items) {
            selected.push_back(item);
            if (selected.size() == limit) {
                break;
            }
        }
        if (*category_filter == ActivityCategory::workflows) {
            remove_top_three_incident_duplicates(category_items, selected);
        }
        return selected;
    }

    if (limit < 3) {
        for (const auto& item : items) {
            selected.push_back(item);
            if (selected.size() == limit) {
                break;
            }
        }
    } else {
        std::array<std::vector<const ActivityItem*>, 3> categories;
        for (const auto& item : items) {
            categories[static_cast<std::size_t>(category(item.kind))].push_back(&item);
        }

        const auto base_quota = limit / categories.size();
        for (const auto& category_items : categories) {
            const auto count = std::min(base_quota, category_items.size());
            for (std::size_t index = 0; index < count; ++index) {
                selected.push_back(*category_items[index]);
            }
        }

        while (selected.size() < limit && selected.size() < items.size()) {
            const auto next =
                std::find_if(items.begin(), items.end(), [&](const ActivityItem& item) {
                    return !selected_contains(selected, item);
                });
            if (next == items.end()) {
                break;
            }
            selected.push_back(*next);
        }
    }

    remove_top_three_incident_duplicates(items, selected);
    return selected;
}

} // namespace ghinfo
