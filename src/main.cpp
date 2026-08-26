#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/poller.hpp"
#include "ghinfo/server.hpp"
#include "ghinfo/snapshot.hpp"

#include <exception>
#include <iostream>
#include <string_view>
#include <thread>

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

        std::cout << "ghinfo " << GHINFO_VERSION << " listening on " << config.bind_address << ':'
                  << config.port << '\n';

        if (!server.listen()) {
            std::cerr << "failed to start HTTP server\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
