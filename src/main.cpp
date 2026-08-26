#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/poller.hpp"
#include "ghinfo/server.hpp"
#include "ghinfo/snapshot.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

volatile std::sig_atomic_t shutdown_requested = 0;

void request_shutdown(int) noexcept {
    shutdown_requested = 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "ghinfo " << GHINFO_VERSION << '\n';
        return 0;
    }

    try {
        const auto config = ghinfo::load_config_from_environment();
        ghinfo::GitHubClient github{config.github_token};
        ghinfo::SnapshotStore store;
        ghinfo::ApiServer server{config, store};
        ghinfo::Poller poller{config, github, store};
        std::jthread poller_thread{
            [&poller](std::stop_token stop_token) { poller.run(stop_token); }};

        std::signal(SIGINT, request_shutdown);
        std::signal(SIGTERM, request_shutdown);

        std::atomic<bool> listen_finished{false};
        std::atomic<bool> listen_succeeded{false};
        std::jthread server_thread{[&server, &listen_finished, &listen_succeeded] {
            const auto succeeded = server.listen();
            listen_succeeded.store(succeeded, std::memory_order_release);
            listen_finished.store(true, std::memory_order_release);
        }};

        std::cout << "ghinfo " << GHINFO_VERSION << " listening on " << config.bind_address << ':'
                  << config.port << '\n';

        while (shutdown_requested == 0 && !listen_finished.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }

        server.stop();
        server_thread.join();
        if (!listen_succeeded.load(std::memory_order_acquire)) {
            std::cerr << "failed to start HTTP server\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
