#include "ghinfo/github_client.hpp"

#include <httplib.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::string read_fixture(const std::string& relative_path) {
    std::ifstream file{std::string{GHINFO_SOURCE_DIR} + "/" + relative_path};
    if (!file) {
        throw std::runtime_error("failed to read test fixture");
    }
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

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

TEST(GitHubClientTest, ReusesCachedBodyAfterNotModifiedResponse) {
    LocalHttpServer server;
    std::atomic<int> request_count{0};
    server.server().Get("/cached",
                        [&](const httplib::Request& request, httplib::Response& response) {
                            const auto current_request = request_count.fetch_add(1);
                            if (current_request == 0) {
                                if (request.has_header("If-None-Match")) {
                                    response.status = 400;
                                    return;
                                }
                                response.set_header("ETag", "\"cached-v1\"");
                                response.set_header("X-RateLimit-Remaining", "100");
                                response.set_content("{\"version\":1}", "application/json");
                                return;
                            }

                            if (request.get_header_value("If-None-Match") != "\"cached-v1\"") {
                                response.status = 400;
                                return;
                            }
                            response.status = 304;
                            response.set_header("ETag", "\"cached-v1\"");
                            response.set_header("X-RateLimit-Remaining", "98");
                        });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto first = client.get("/cached");
    const auto second = client.get("/cached");

    EXPECT_EQ(request_count.load(), 2);
    EXPECT_EQ(first.status_code, 200);
    EXPECT_EQ(second.status_code, 304);
    EXPECT_EQ(second.body, first.body);
    ASSERT_TRUE(second.header("etag").has_value());
    EXPECT_EQ(second.header("etag").value(), "\"cached-v1\"");
    ASSERT_TRUE(second.header("x-ratelimit-remaining").has_value());
    EXPECT_EQ(second.header("x-ratelimit-remaining").value(), "98");
}

TEST(GitHubClientTest, RejectsNotModifiedResponseWithoutCachedBody) {
    LocalHttpServer server;
    server.server().Get("/uncached", [](const httplib::Request&, httplib::Response& response) {
        response.status = 304;
    });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    try {
        (void)client.get("/uncached");
        FAIL() << "expected HTTP error";
    } catch (const ghinfo::GitHubRequestError& error) {
        EXPECT_EQ(error.kind(), ghinfo::GitHubErrorKind::http);
        ASSERT_TRUE(error.status_code().has_value());
        EXPECT_EQ(error.status_code().value(), 304);
        EXPECT_EQ(std::string{error.what()},
                  "GitHub returned HTTP 304 without a cached response for /uncached");
    }
}

TEST(GitHubClientTest, FetchesAndNormalizesPaginatedOpenIssues) {
    LocalHttpServer server;
    const auto first_page = read_fixture("tests/fixtures/github/issues.json");
    std::atomic<int> request_count{0};
    server.server().Get("/repos/owner/repo/issues", [&](const httplib::Request& request,
                                                        httplib::Response& response) {
        ++request_count;
        const auto page = request.get_param_value("page");
        if (page == "1") {
            response.set_header("Link", "<http://example.test/page=2>; rel=\"next\"");
            response.set_content(first_page, "application/json");
            return;
        }
        if (page == "2") {
            response.set_content("[]", "application/json");
            return;
        }
        response.status = 400;
    });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto issues = client.fetch_open_issues(ghinfo::parse_repository_ref("owner/repo"));

    ASSERT_EQ(request_count.load(), 2);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues.front().id, 1001U);
    EXPECT_EQ(issues.front().number, 42U);
    EXPECT_EQ(issues.front().repository, "owner/repo");
    EXPECT_EQ(issues.front().title, "Example issue");
    EXPECT_EQ(issues.front().author, "octocat");
    ASSERT_EQ(issues.front().labels.size(), 1U);
    EXPECT_EQ(issues.front().labels.front(), "bug");
    EXPECT_EQ(issues.front().url, "https://github.com/owner/repo/issues/42");
}

TEST(GitHubClientTest, ReportsMalformedIssuePayload) {
    LocalHttpServer server;
    server.server().Get("/repos/owner/repo/issues",
                        [](const httplib::Request&, httplib::Response& response) {
                            response.set_content("not-json", "application/json");
                        });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    try {
        (void)client.fetch_open_issues(ghinfo::parse_repository_ref("owner/repo"));
        FAIL() << "expected malformed JSON error";
    } catch (const ghinfo::GitHubRequestError& error) {
        EXPECT_EQ(error.kind(), ghinfo::GitHubErrorKind::malformed_json);
        EXPECT_FALSE(error.status_code().has_value());
    }
}

TEST(GitHubClientTest, FetchesAndNormalizesPaginatedOpenPullRequests) {
    LocalHttpServer server;
    const auto first_page = read_fixture("tests/fixtures/github/pulls.json");
    std::atomic<int> request_count{0};
    server.server().Get("/repos/owner/repo/pulls", [&](const httplib::Request& request,
                                                       httplib::Response& response) {
        ++request_count;
        const auto page = request.get_param_value("page");
        if (page == "1") {
            response.set_header("Link", "<http://example.test/page=2>; rel=\"next\"");
            response.set_content(first_page, "application/json");
            return;
        }
        if (page == "2") {
            response.set_content("[]", "application/json");
            return;
        }
        response.status = 400;
    });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto pull_requests =
        client.fetch_open_pull_requests(ghinfo::parse_repository_ref("owner/repo"));

    ASSERT_EQ(request_count.load(), 2);
    ASSERT_EQ(pull_requests.size(), 1U);
    EXPECT_EQ(pull_requests.front().id, 2001U);
    EXPECT_EQ(pull_requests.front().number, 17U);
    EXPECT_EQ(pull_requests.front().repository, "owner/repo");
    EXPECT_EQ(pull_requests.front().title, "Example pull request");
    EXPECT_EQ(pull_requests.front().author, "octocat");
    EXPECT_FALSE(pull_requests.front().draft);
    EXPECT_EQ(pull_requests.front().head, "feature/example");
    EXPECT_EQ(pull_requests.front().base, "main");
    EXPECT_EQ(pull_requests.front().url, "https://github.com/owner/repo/pull/17");
}

TEST(GitHubClientTest, FetchesBoundedWorkflowRunHistory) {
    LocalHttpServer server;
    const auto fixture = read_fixture("tests/fixtures/github/runs.json");
    server.server().Get("/repos/owner/repo/actions/runs", [&](const httplib::Request& request,
                                                              httplib::Response& response) {
        if (request.get_param_value("per_page") != "20" || request.get_param_value("page") != "1") {
            response.status = 400;
            return;
        }
        response.set_content(fixture, "application/json");
    });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };

    const auto runs = client.fetch_workflow_runs(ghinfo::parse_repository_ref("owner/repo"), 20);

    ASSERT_EQ(runs.size(), 1U);
    EXPECT_EQ(runs.front().id, 3001U);
    EXPECT_EQ(runs.front().repository, "owner/repo");
    EXPECT_EQ(runs.front().name, "CI");
    EXPECT_EQ(runs.front().status, ghinfo::RunStatus::completed);
    ASSERT_TRUE(runs.front().conclusion.has_value());
    EXPECT_EQ(runs.front().conclusion.value(), ghinfo::Conclusion::failure);
    EXPECT_EQ(runs.front().branch, "main");
    EXPECT_EQ(runs.front().commit_sha, "0123456789abcdef");
    EXPECT_EQ(runs.front().event, "push");
    EXPECT_EQ(runs.front().url, "https://github.com/owner/repo/actions/runs/3001");
}

TEST(GitHubClientTest, RejectsInvalidWorkflowHistoryLimit) {
    ghinfo::GitHubClient client{"test-token"};

    EXPECT_THROW((void)client.fetch_workflow_runs(ghinfo::parse_repository_ref("owner/repo"), 0),
                 std::invalid_argument);
    EXPECT_THROW((void)client.fetch_workflow_runs(ghinfo::parse_repository_ref("owner/repo"), 101),
                 std::invalid_argument);
}

TEST(GitHubClientTest, FetchesJobsOnlyForRelevantRuns) {
    LocalHttpServer server;
    const auto first_page = read_fixture("tests/fixtures/github/jobs.json");
    std::atomic<int> request_count{0};
    server.server().Get(
        "/repos/owner/repo/actions/runs/3001/jobs",
        [&](const httplib::Request& request, httplib::Response& response) {
            ++request_count;
            if (request.get_param_value("per_page") != "100" ||
                request.get_param_value("page") == "") {
                response.status = 400;
                return;
            }
            if (request.get_param_value("page") == "1") {
                response.set_header("Link", "<http://example.test/page=2>; rel=\"next\"");
                response.set_content(first_page, "application/json");
                return;
            }
            response.set_content("{\"total_count\":0,\"jobs\":[]}", "application/json");
        });

    ghinfo::GitHubClient client{
        "test-token",
        ghinfo::GitHubClientOptions{.base_url = server.base_url()},
    };
    const auto repository = ghinfo::parse_repository_ref("owner/repo");

    ghinfo::WorkflowRun failed_run;
    failed_run.id = 3001;
    failed_run.status = ghinfo::RunStatus::completed;
    failed_run.conclusion = ghinfo::Conclusion::failure;
    const auto jobs = client.fetch_relevant_workflow_jobs(repository, failed_run);

    ASSERT_EQ(request_count.load(), 2);
    ASSERT_EQ(jobs.size(), 1U);
    EXPECT_EQ(jobs.front().id, 4001U);
    EXPECT_EQ(jobs.front().run_id, 3001U);
    EXPECT_EQ(jobs.front().repository, "owner/repo");
    EXPECT_EQ(jobs.front().name, "gcc-debug");
    EXPECT_EQ(jobs.front().status, ghinfo::RunStatus::completed);
    ASSERT_TRUE(jobs.front().conclusion.has_value());
    EXPECT_EQ(jobs.front().conclusion.value(), ghinfo::Conclusion::failure);
    ASSERT_TRUE(jobs.front().started_at.has_value());
    EXPECT_EQ(jobs.front().started_at.value(), "2026-08-26T12:00:10Z");
    EXPECT_EQ(jobs.front().url, "https://github.com/owner/repo/actions/runs/3001/job/4001");

    ghinfo::WorkflowRun successful_run;
    successful_run.id = 3002;
    successful_run.status = ghinfo::RunStatus::completed;
    successful_run.conclusion = ghinfo::Conclusion::success;
    EXPECT_TRUE(client.fetch_relevant_workflow_jobs(repository, successful_run).empty());
    EXPECT_EQ(request_count.load(), 2);
}

} // namespace
