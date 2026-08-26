#include "ghinfo/config.hpp"
#include "ghinfo/github_client.hpp"
#include "ghinfo/server.hpp"
#include "ghinfo/snapshot.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "ghinfo " << GHINFO_VERSION << '\n';
        return 0;
    }

    try {
        const auto config = ghinfo::load_config_from_environment();
        ghinfo::GitHubClient github{config.github_token};
        ghinfo::SnapshotStore store;

        // The client is intentionally constructed now so invalid secret/config state
        // fails fast. Background polling is added in the roadmap after transport/resources.
        (void)github;

        ghinfo::ApiServer server{config, store};

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
