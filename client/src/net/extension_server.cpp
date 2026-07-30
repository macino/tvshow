#include "tvshow/net/extension_server.hpp"

#include "tvshow/app/extension_process.hpp"

#include <httplib.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace tvshow::net {

namespace {

namespace fs = std::filesystem;

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// Marker file recording a successful install, so re-opening tvshow doesn't
// re-run `install` on every request. Keyed by extension dir, not just name,
// so bundled and user-installed extensions of the same name can't collide.
fs::path state_dir() {
    const char* xdg = ::getenv("XDG_STATE_HOME");
    const fs::path base = (xdg && xdg[0] != '\0')
                              ? fs::path(xdg)
                              : fs::path(::getenv("HOME") ? ::getenv("HOME") : "/tmp") / ".local/state";
    return base / "tvshow" / "extensions-installed";
}

std::string marker_name(const std::string& dir) {
    return std::to_string(std::hash<std::string>{}(dir));
}

}  // namespace

ExtensionServer::ExtensionServer(std::vector<std::string> extension_dirs)
    : server_(std::make_unique<httplib::Server>()) {
    for (const auto& root : extension_dirs) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) { continue; }
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (!entry.is_directory()) { continue; }
            const fs::path manifest_path = entry.path() / "extension.toml";
            const std::string toml = read_file(manifest_path);
            if (toml.empty()) { continue; }
            auto manifest = parse_extension_manifest(toml);
            if (!manifest) { continue; }
            extensions_.push_back(
                Extension{std::move(*manifest), entry.path().string(), false});
        }
    }
}

ExtensionServer::~ExtensionServer() {
    if (server_) { server_->stop(); }
    if (thread_.joinable()) { thread_.join(); }
}

bool ExtensionServer::ensure_installed(Extension& ext) {
    if (ext.installed) { return true; }
    if (ext.manifest.install.empty()) {
        ext.installed = true;
        return true;
    }

    std::error_code ec;
    const fs::path marker = state_dir() / marker_name(ext.dir);
    if (fs::exists(marker, ec)) {
        ext.installed = true;
        return true;
    }

    fs::create_directories(marker.parent_path(), ec);
    const std::string cmd = "cd '" + ext.dir + "' && " + ext.manifest.install;
    const int rc = std::system(cmd.c_str());  // NOLINT(cert-env33-c,concurrency-mt-unsafe)
    if (rc != 0) { return false; }

    std::ofstream(marker).close();
    ext.installed = true;
    return true;
}

CgiResponse ExtensionServer::run_extension(const Extension& ext, std::string_view method,
                                           std::string_view path, std::string_view query,
                                           std::string_view body) {
    const std::string cmd = "cd '" + ext.dir + "' && " + ext.manifest.entry;
    app::ExtensionProcess proc({"/bin/sh", "-c", cmd});

    proc.write(build_cgi_request(method, path, query, body));
    proc.close_stdin();

    std::string out;
    // Blocking-ish wait, on the server's own background thread (never the
    // TUI thread) -- a short poll loop is fine here, unlike the non-blocking
    // discipline ExtensionProcess's other callers need for the UI event loop.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (proc.alive() && std::chrono::steady_clock::now() < deadline) {
        out += proc.read_available();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    out += proc.read_available();

    if (out.empty()) {
        return CgiResponse{502, {{"Content-Type", "text/plain"}}, "extension produced no output"};
    }
    return parse_cgi_response(out);
}

bool ExtensionServer::start(int port) {
    if (running_) { return true; }

    auto handle = [this](const httplib::Request& req, httplib::Response& res) {
        const std::string name = req.matches[1].str();
        const std::string rest = req.matches.size() > 2 ? req.matches[2].str() : "";

        Extension* ext = nullptr;
        for (auto& candidate : extensions_) {
            if (candidate.manifest.name == name) { ext = &candidate; break; }
        }
        if (ext == nullptr) {
            res.status = 404;
            res.set_content("extension not found: " + name, "text/plain");
            return;
        }
        if (!ensure_installed(*ext)) {
            res.status = 503;
            res.set_content("extension install failed: " + name, "text/plain");
            return;
        }

        std::string query;
        for (const auto& [key, value] : req.params) {
            if (!query.empty()) { query += "&"; }
            query += key + "=" + value;
        }

        const CgiResponse cgi =
            run_extension(*ext, req.method, rest.empty() ? "/" : rest, query, req.body);
        res.status = cgi.status;
        std::string content_type = "text/html";
        for (const auto& [key, value] : cgi.headers) {
            if (key == "Content-Type") {
                content_type = value;
            } else {
                res.set_header(key, value);
            }
        }
        res.set_content(cgi.body, content_type);
    };

    server_->Get(R"(/extensions/([^/]+)(/.*)?)", handle);
    server_->Post(R"(/extensions/([^/]+)(/.*)?)", handle);

    if (!server_->bind_to_port("127.0.0.1", port)) { return false; }
    port_ = port;

    thread_ = std::thread([this] { server_->listen_after_bind(); });
    running_ = true;
    return true;
}

}  // namespace tvshow::net
