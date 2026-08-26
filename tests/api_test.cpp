#include "ghinfo/server.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

TEST(ApiTest, HealthPayloadIsAlwaysOk) {
    const auto response = ghinfo::make_health_response();

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("status"), "ok");
}

TEST(ApiTest, ReadyPayloadReturns503BeforeSnapshot) {
    const auto response = ghinfo::make_readiness_response(false);

    EXPECT_EQ(response.status, 503);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_FALSE(body.at("ready").get<bool>());
}

TEST(ApiTest, ReadyPayloadReturns200AfterSnapshot) {
    const auto response = ghinfo::make_readiness_response(true);

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_TRUE(body.at("ready").get<bool>());
}

TEST(ApiTest, MetaPayloadIsVersioned) {
    const auto response = ghinfo::make_meta_response(false);

    EXPECT_EQ(response.status, 200);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("schemaVersion"), 1);
    EXPECT_EQ(body.at("service"), "ghinfo");
    EXPECT_FALSE(body.at("snapshotAvailable").get<bool>());
}

} // namespace
