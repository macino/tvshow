// tvshow-srv — demo HTTP server (SPEC §16).
// Serves the static sample pages under pages/ plus a couple of dynamic
// routes (/echo, /pages/errors/404) used by integration tests.

#include <httplib.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string read_file(const std::string& path) {
    const std::ifstream in(path, std::ios::binary);
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

void serve_page(httplib::Response& res, const std::string& path) {
    const std::string body = read_file(path);
    if (body.empty()) {
        res.status = 404;
        res.set_content("<!doctype html><html><body><h1>404 Not Found</h1></body></html>",
                        "text/html");
        return;
    }
    res.set_content(body, "text/html");
}

std::string html_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string render_echo(const httplib::Request& req) {
    std::ostringstream html;
    html << "<!doctype html><html><head><title>Echo</title></head><body>";
    html << "<h1>Submitted form</h1><ul>";
    for (const auto& [key, value] : req.params) {
        html << "<li>" << html_escape(key) << " = " << html_escape(value) << "</li>";
    }
    html << "</ul></body></html>";
    return html.str();
}

}  // namespace

auto main(int argc, char** argv) -> int {
    int port = 8080;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        }
    }

    const std::string pages_dir = TVSHOW_PAGES_DIR;
    httplib::Server svr;

    svr.Get("/", [pages_dir](const httplib::Request&, httplib::Response& res) {
        serve_page(res, pages_dir + "/index.html");
    });

    // Match /pages/<path>.html where <path> may include subdirectories.
    // The character class allows word chars, hyphens, and forward slashes;
    // a leading .. is rejected to prevent path traversal.
    svr.Get(R"(/pages/([\w][\w/\-]*)\.html)",
            [pages_dir](const httplib::Request& req, httplib::Response& res) {
                const std::string rel = req.matches[1].str();
                // Reject any attempt at path traversal.
                if (rel.find("..") != std::string::npos) {
                    res.status = 400;
                    return;
                }
                serve_page(res, pages_dir + "/" + rel + ".html");
            });

    // Serve CSS stylesheets from pages/styles/.
    svr.Get(R"(/styles/([\w][\w\-]*)\.css)",
            [pages_dir](const httplib::Request& req, httplib::Response& res) {
                const std::string rel = req.matches[1].str();
                if (rel.find("..") != std::string::npos) {
                    res.status = 400;
                    return;
                }
                const std::string body = read_file(pages_dir + "/styles/" + rel + ".css");
                if (body.empty()) {
                    res.status = 404;
                    return;
                }
                res.set_content(body, "text/css");
            });

    svr.Get("/pages/errors/404", [](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content("<!doctype html><html><body><h1>404 Not Found</h1>"
                        "<p>This page intentionally does not exist.</p></body></html>",
                        "text/html");
    });

    svr.Get("/echo", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(render_echo(req), "text/html");
    });

    svr.Post("/echo", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(render_echo(req), "text/html");
    });

    // port 0 asks the OS for a free port (used by integration tests, which
    // spawn this binary and parse the bound port back out of stdout).
    if (port == 0) {
        const int bound = svr.bind_to_any_port("127.0.0.1");
        if (bound <= 0) {
            std::fprintf(stderr, "tvshow-srv: failed to bind an ephemeral port\n");
            return 1;
        }
        std::printf("tvshow-srv: listening on http://127.0.0.1:%d/ (pages: %s)\n", bound,
                    pages_dir.c_str());
        std::fflush(stdout);
        svr.listen_after_bind();
        return 0;
    }

    std::printf("tvshow-srv: listening on http://0.0.0.0:%d/ (pages: %s)\n", port,
                pages_dir.c_str());
    std::fflush(stdout);
    svr.listen("0.0.0.0", port);
    return 0;
}
