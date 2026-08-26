#include "ghinfo/github_client.hpp"

#include <httplib.h>

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

class LocalHttpServer {
  public:
    LocalHttpServer() {
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) {
            throw std::runtime_error("failed to bind local test server");
        }
        thread_ = std::thread([this] { server_.listen_after_bind(); });

        for (int attempt = 0; attempt < 100 && !server_.is_running(); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        if (!server_.is_running()) {
            server_.stop();
            thread_.join();
            throw std::runtime_error("local test server did not start");
        }
    }

    ~LocalHttpServer() {
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    httplib::Server& server() {
        return server_;
    }

  private:
    httplib::Server server_;
    std::thread thread_;
    int port_{};
};

TEST(GitHubClientTest, RejectsEmptyToken) {
    EXPECT_THROW((void)ghinfo::GitHubClient{""}, std::invalid_argument);
}

TEST(GitHubClientTest, DefinesPinnedTransportIdentity) {
    EXPECT_EQ(ghinfo::GitHubClient::api_base, "https://api.github.com");
    EXPECT_EQ(ghinfo::GitHubClient::api_version, "2026-03-10");
    EXPECT_EQ(ghinfo::GitHubClient::user_agent, "ghinfo/0.1.0");
}

TEST(GitHubClientTest, SendsAuthenticatedRequestAndCapturesResponse) {
    LocalHttpServer server;
    server.server().Get("/test", [](const httplib::Request& request, httplib::Response& response) {
        if (request.get_header_value("Authorization") != "Bearer test-token" ||
            request.get_header_value("Accept") != "application/vnd.github+json" ||
            request.get_header_value("X-GitHub-Api-Version") != "2026-03-10" ||
            request.get_header_value("User-Agent") != "ghinfo/0.1.0") {
            response.status = 400;
            return;
        }
        response.set_header("ETag", "\"test-etag\"");
        response.set_content("{\"ok\":true}", "application/json");
    });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{
            .base_url = server.base_url(),
            .connect_timeout = std::chrono::milliseconds{1000},
            .total_timeout = std::chrono::milliseconds{1000},
        },
    };

    const auto response = client.get("/test");

    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "{\"ok\":true}");
    ASSERT_TRUE(response.header("etag").has_value());
    EXPECT_EQ(response.header("etag").value(), "\"test-etag\"");
}

TEST(GitHubClientTest, SeparatesHttpErrorsWithoutExposingTokenOrBody) {
    LocalHttpServer server;
    server.server().Get("/forbidden", [](const httplib::Request&, httplib::Response& response) {
        response.status = 403;
        response.set_content("upstream body must not escape", "text/plain");
    });

    constexpr std::string_view token = "secret-test-token";
    ghinfo::GitHubClient client{
        std::string{token},
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    try {
        (void)client.get("/forbidden");
        FAIL() << "expected HTTP error";
    } catch (const ghinfo::GitHubRequestError& error) {
        EXPECT_EQ(error.kind(), ghinfo::GitHubErrorKind::http);
        ASSERT_TRUE(error.status_code().has_value());
        EXPECT_EQ(error.status_code().value(), 403);
        EXPECT_EQ(std::string{error.what()}, "GitHub returned HTTP 403 for /forbidden");
        EXPECT_EQ(std::string{error.what()}.find(token), std::string::npos);
        EXPECT_EQ(std::string{error.what()}.find("upstream body"), std::string::npos);
    }
}

TEST(GitHubClientTest, SeparatesTransportErrors) {
    ghinfo::GitHubClient client{
        "secret-test-token",
        ghinfo::GitHubClientOptions{
            .base_url = "http://127.0.0.1:1",
            .connect_timeout = std::chrono::milliseconds{100},
            .total_timeout = std::chrono::milliseconds{100},
        },
    };

    try {
        (void)client.get("/unreachable");
        FAIL() << "expected transport error";
    } catch (const ghinfo::GitHubRequestError& error) {
        EXPECT_EQ(error.kind(), ghinfo::GitHubErrorKind::transport);
        EXPECT_FALSE(error.status_code().has_value());
        EXPECT_EQ(std::string{error.what()}.find("secret-test-token"), std::string::npos);
    }
}

} // namespace
