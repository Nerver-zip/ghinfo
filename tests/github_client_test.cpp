#include "ghinfo/github_client.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

TEST(GitHubClientTest, RejectsEmptyToken) {
    EXPECT_THROW((void)ghinfo::GitHubClient{""}, std::invalid_argument);
}

TEST(GitHubClientTest, DefinesPinnedTransportIdentity) {
    EXPECT_EQ(ghinfo::GitHubClient::api_base, "https://api.github.com");
    EXPECT_EQ(ghinfo::GitHubClient::api_version, "2026-03-10");
    EXPECT_EQ(ghinfo::GitHubClient::user_agent, "ghinfo/0.1.0");
}

} // namespace
