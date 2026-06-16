#pragma once

#include <string>

namespace tvshow::itest {

// Spawns tvshow-srv (path baked in via TVSHOW_SRV_PATH) on an ephemeral
// port for the lifetime of the fixture, and tears it down (SIGTERM + wait)
// on destruction. SPEC §17.1: integration tests spawn the demo server on
// an ephemeral port and drive the client library API against it.
class ServerFixture {
public:
    ServerFixture();
    ~ServerFixture();

    ServerFixture(const ServerFixture&) = delete;
    ServerFixture& operator=(const ServerFixture&) = delete;
    ServerFixture(ServerFixture&&) = delete;
    ServerFixture& operator=(ServerFixture&&) = delete;

    [[nodiscard]] const std::string& base_url() const noexcept { return base_url_; }

private:
    int pid_ = -1;
    std::string base_url_;
};

}  // namespace tvshow::itest
