#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ghinfo {

struct RepositoryRef {
    std::string owner;
    std::string name;

    [[nodiscard]] std::string full_name() const;
    friend bool operator==(const RepositoryRef&, const RepositoryRef&) = default;
};

enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error,
};

enum class RepositorySelection {
    explicit_list,
    discover_all,
};

struct Config {
    std::string github_token;
    RepositorySelection repository_selection{RepositorySelection::explicit_list};
    std::vector<RepositoryRef> repositories;
    std::uint32_t poll_interval_seconds{60};
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{8080};
    LogLevel log_level{LogLevel::info};
    std::uint32_t run_history{20};
};

[[nodiscard]] Config load_config_from_environment();
[[nodiscard]] RepositoryRef parse_repository_ref(const std::string& value);
[[nodiscard]] RepositorySelection parse_repository_selection(const std::string& value);
[[nodiscard]] LogLevel parse_log_level(const std::string& value);
[[nodiscard]] std::string to_string(LogLevel level);

} // namespace ghinfo
