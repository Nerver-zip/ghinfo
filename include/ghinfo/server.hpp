#pragma once

#include "ghinfo/config.hpp"
#include "ghinfo/snapshot.hpp"

#include <httplib.h>

#include <string>

namespace ghinfo {

struct JsonResponse {
    int status{200};
    std::string body;
};

[[nodiscard]] JsonResponse make_health_response();
[[nodiscard]] JsonResponse make_readiness_response(bool ready);
[[nodiscard]] JsonResponse make_meta_response(bool snapshot_available);

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
