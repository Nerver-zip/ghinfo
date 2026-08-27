#include "ghinfo/config.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

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

} // namespace
