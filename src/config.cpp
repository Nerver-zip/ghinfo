#include "ghinfo/config.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace ghinfo {
namespace {

[[nodiscard]] std::string getenv_required(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string_view{value}.empty()) {
        throw std::runtime_error(std::string{name} + " is required");
    }
    return value;
}

[[nodiscard]] std::string getenv_or(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string_view{value}.empty()) {
        return fallback;
    }
    return value;
}

template <typename T>
[[nodiscard]] T parse_integer(const std::string& text, const char* name, T min, T max) {
    T value{};
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);

    if (ec != std::errc{} || ptr != end || value < min || value > max) {
        throw std::runtime_error(std::string{name} + " has an invalid value");
    }
    return value;
}

[[nodiscard]] std::vector<RepositoryRef> parse_repositories(const std::string& value) {
    std::vector<RepositoryRef> repositories;
    std::size_t begin = 0;

    while (begin <= value.size()) {
        const auto end = value.find(',', begin);
        const auto token =
            value.substr(begin, end == std::string::npos ? std::string::npos : end - begin);

        if (token.empty()) {
            throw std::runtime_error("GHINFO_REPOSITORIES contains an empty repository");
        }

        repositories.push_back(parse_repository_ref(token));

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    if (repositories.empty()) {
        throw std::runtime_error("GHINFO_REPOSITORIES must contain at least one repository");
    }

    return repositories;
}

} // namespace

std::string RepositoryRef::full_name() const {
    return owner + "/" + name;
}

RepositoryRef parse_repository_ref(const std::string& value) {
    const auto slash = value.find('/');

    if (slash == std::string::npos || slash == 0 || slash == value.size() - 1 ||
        value.find('/', slash + 1) != std::string::npos) {
        throw std::runtime_error("repository must use owner/name syntax");
    }

    RepositoryRef result{value.substr(0, slash), value.substr(slash + 1)};

    const auto valid = [](std::string_view part) {
        return std::all_of(part.begin(), part.end(), [](unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
        });
    };

    if (!valid(result.owner) || !valid(result.name)) {
        throw std::runtime_error("repository contains unsupported characters");
    }

    return result;
}

LogLevel parse_log_level(const std::string& value) {
    if (value == "trace") {
        return LogLevel::trace;
    }
    if (value == "debug") {
        return LogLevel::debug;
    }
    if (value == "info") {
        return LogLevel::info;
    }
    if (value == "warn") {
        return LogLevel::warn;
    }
    if (value == "error") {
        return LogLevel::error;
    }
    throw std::runtime_error("GHINFO_LOG_LEVEL must be trace, debug, info, warn, or error");
}

std::string to_string(LogLevel level) {
    switch (level) {
    case LogLevel::trace:
        return "trace";
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warn:
        return "warn";
    case LogLevel::error:
        return "error";
    }
    return "unknown";
}

Config load_config_from_environment() {
    Config config;
    config.github_token = getenv_required("GHINFO_GITHUB_TOKEN");
    const auto repository_value = getenv_required("GHINFO_REPOSITORIES");
    config.repository_selection = parse_repository_selection(repository_value);
    if (config.repository_selection == RepositorySelection::explicit_list) {
        config.repositories = parse_repositories(repository_value);
    }

    config.poll_interval_seconds = parse_integer<std::uint32_t>(
        getenv_or("GHINFO_POLL_INTERVAL_SECONDS", "60"), "GHINFO_POLL_INTERVAL_SECONDS", 5U, 3600U);

    config.bind_address = getenv_or("GHINFO_BIND", "127.0.0.1");

    config.port = parse_integer<std::uint16_t>(getenv_or("GHINFO_PORT", "8080"), "GHINFO_PORT",
                                               static_cast<std::uint16_t>(1),
                                               std::numeric_limits<std::uint16_t>::max());

    config.log_level = parse_log_level(getenv_or("GHINFO_LOG_LEVEL", "info"));

    config.run_history = parse_integer<std::uint32_t>(getenv_or("GHINFO_RUN_HISTORY", "20"),
                                                      "GHINFO_RUN_HISTORY", 1U, 100U);

    return config;
}

RepositorySelection parse_repository_selection(const std::string& value) {
    return value == "auto" ? RepositorySelection::discover_all : RepositorySelection::explicit_list;
}

} // namespace ghinfo
