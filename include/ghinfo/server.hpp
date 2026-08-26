#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/snapshot.hpp"

#include <httplib.h>

#include <optional>
#include <string>

namespace ghinfo {

struct JsonResponse {
    int status{200};
    std::string body;
};

[[nodiscard]] JsonResponse make_health_response();
[[nodiscard]] JsonResponse make_readiness_response(bool ready);
[[nodiscard]] JsonResponse make_meta_response(bool snapshot_available);
[[nodiscard]] JsonResponse make_meta_response(const SnapshotStore& store);
[[nodiscard]] JsonResponse make_summary_response(const SnapshotStore& store);
[[nodiscard]] JsonResponse make_repositories_response(const Snapshot& snapshot);
[[nodiscard]] JsonResponse make_repository_response(const Snapshot& snapshot,
                                                    const std::string& full_name);
[[nodiscard]] JsonResponse make_issues_response(const Snapshot& snapshot,
                                                const std::optional<std::string>& repository);
[[nodiscard]] JsonResponse
make_pull_requests_response(const Snapshot& snapshot, const std::optional<std::string>& repository);
[[nodiscard]] JsonResponse
make_workflow_runs_response(const Snapshot& snapshot, const std::optional<std::string>& repository,
                            const std::optional<std::string>& status,
                            const std::optional<std::string>& conclusion);
[[nodiscard]] JsonResponse
make_workflow_jobs_response(const Snapshot& snapshot, const std::optional<std::string>& repository);
[[nodiscard]] JsonResponse make_activity_response(const SnapshotStore& store);
[[nodiscard]] JsonResponse make_snapshot_unavailable_response();

class ApiServer {
  public:
    ApiServer(const Config& config, const SnapshotStore& store);

    [[nodiscard]] bool listen();
    void stop();

  private:
    const Config& config_;
    const SnapshotStore& store_;
    httplib::Server server_;

    void register_routes();
};

} // namespace ghinfo
