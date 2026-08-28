#include "ghinfo/config.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

class ScopedEnvironment {
  public:
    ScopedEnvironment(const char* name, const char* value) : name_(name) {
        if (const char* previous = std::getenv(name); previous != nullptr) {
            previous_ = previous;
        }
        if (setenv(name, value, 1) != 0) {
            throw std::runtime_error("failed to set test environment");
        }
    }

    ~ScopedEnvironment() {
        if (previous_.has_value()) {
            static_cast<void>(setenv(name_.c_str(), previous_->c_str(), 1));
        } else {
            static_cast<void>(unsetenv(name_.c_str()));
        }
    }

  private:
    std::string name_;
    std::optional<std::string> previous_;
};

void set_required_environment() {
    static ScopedEnvironment token{"GHINFO_GITHUB_TOKEN", "test-token"};
    static ScopedEnvironment repositories{"GHINFO_REPOSITORIES", "owner/repo"};
}

TEST(ConfigTest, ParsesRepositoryReference) {
    const auto repo = ghinfo::parse_repository_ref("Nerver-zip/chess-tactics");

    EXPECT_EQ(repo.owner, "Nerver-zip");
    EXPECT_EQ(repo.name, "chess-tactics");
    EXPECT_EQ(repo.full_name(), "Nerver-zip/chess-tactics");
}

TEST(ConfigTest, RejectsMalformedRepositoryReference) {
    EXPECT_THROW((void)ghinfo::parse_repository_ref("missing-slash"), std::runtime_error);
    EXPECT_THROW((void)ghinfo::parse_repository_ref("/repo"), std::runtime_error);
    EXPECT_THROW((void)ghinfo::parse_repository_ref("owner/"), std::runtime_error);
    EXPECT_THROW((void)ghinfo::parse_repository_ref("a/b/c"), std::runtime_error);
}

TEST(ConfigTest, SupportsAutomaticRepositorySelection) {
    EXPECT_EQ(ghinfo::parse_repository_selection("auto"),
              ghinfo::RepositorySelection::discover_all);
    EXPECT_EQ(ghinfo::parse_repository_selection("owner/repo"),
              ghinfo::RepositorySelection::explicit_list);
}

TEST(ConfigTest, ParsesLogLevels) {
    EXPECT_EQ(ghinfo::to_string(ghinfo::parse_log_level("debug")), "debug");
    EXPECT_EQ(ghinfo::to_string(ghinfo::parse_log_level("warn")), "warn");
    EXPECT_THROW((void)ghinfo::parse_log_level("verbose"), std::runtime_error);
}

TEST(ConfigTest, UsesTenRecentJobRunsByDefault) {
    set_required_environment();
    ScopedEnvironment job_history{"GHINFO_JOB_RUN_HISTORY", ""};
    static_cast<void>(unsetenv("GHINFO_JOB_RUN_HISTORY"));

    const auto config = ghinfo::load_config_from_environment();

    EXPECT_EQ(config.run_history, 20U);
    EXPECT_EQ(config.job_run_history, 10U);
}

TEST(ConfigTest, ValidatesRecentJobRunWindow) {
    set_required_environment();
    ScopedEnvironment job_history{"GHINFO_JOB_RUN_HISTORY", "1"};
    EXPECT_EQ(ghinfo::load_config_from_environment().job_run_history, 1U);

    static_cast<void>(setenv("GHINFO_JOB_RUN_HISTORY", "100", 1));
    EXPECT_EQ(ghinfo::load_config_from_environment().job_run_history, 100U);

    static_cast<void>(setenv("GHINFO_JOB_RUN_HISTORY", "0", 1));
    EXPECT_THROW((void)ghinfo::load_config_from_environment(), std::runtime_error);

    static_cast<void>(setenv("GHINFO_JOB_RUN_HISTORY", "101", 1));
    EXPECT_THROW((void)ghinfo::load_config_from_environment(), std::runtime_error);
}

} // namespace
