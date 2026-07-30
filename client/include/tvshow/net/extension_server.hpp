#pragma once

#include "tvshow/net/extension_manifest.hpp"

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace httplib {
class Server;
}

namespace tvshow::net {

// adr-extension-server: an internal HTTP server, embedded in the tvshow
// client process, that routes /extensions/<name>/... to a CGI-style
// per-request spawn of that extension's `entry` command. Extensions are
// plain HTML (forms, links) served like any other page -- tvshow just
// browses http://127.0.0.1:<port>/extensions/<name>/ as a normal tab, no
// special client-side code needed.
//
// Not a persistent per-extension process: every request is a fresh spawn
// (reusing ExtensionProcess's fork+exec+pipe plumbing), same crash-isolation
// property window-provider extensions already have, at the cost of a
// process-start per request -- acceptable for a demo/skeleton, revisit if
// latency ever matters.
class ExtensionServer {
public:
    // Scans `extension_dirs` for subdirectories containing an
    // extension.toml; each becomes a routable /extensions/<name>/ prefix.
    explicit ExtensionServer(std::vector<std::string> extension_dirs);
    ~ExtensionServer();

    ExtensionServer(const ExtensionServer&) = delete;
    ExtensionServer& operator=(const ExtensionServer&) = delete;
    ExtensionServer(ExtensionServer&&) = delete;
    ExtensionServer& operator=(ExtensionServer&&) = delete;

    // Binds 127.0.0.1:port and starts serving on a background thread.
    // No-op if already running. Returns false if the bind failed.
    bool start(int port);

    // True once start() has successfully bound and the listener thread is up.
    [[nodiscard]] bool running() const { return running_; }

    [[nodiscard]] int port() const { return port_; }

private:
    struct Extension {
        ExtensionManifest manifest;
        std::string dir;
        bool installed = false;
    };

    std::vector<Extension> extensions_;
    std::unique_ptr<httplib::Server> server_;
    std::thread thread_;
    bool running_ = false;
    int port_ = 0;

    // Runs `install`, if any, and marks the extension installed on success.
    // No-op (and treated as already installed) if `install` is empty.
    bool ensure_installed(Extension& ext);

    // Spawns `entry` in `dir`, feeds it a CGI-style request over stdin,
    // blocks until it exits, and parses its stdout as a CGI-style response.
    static CgiResponse run_extension(const Extension& ext, std::string_view method,
                                     std::string_view path, std::string_view query,
                                     std::string_view body);
};

}  // namespace tvshow::net
