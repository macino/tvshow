#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::net {

// adr-extension-server: one `extension.toml` per extension directory.
// `entry` is the command run per HTTP request (cwd = the extension's own
// directory, so relative paths like "server.py" work); `install`, if
// non-empty, is run once before the extension is routed for the first time.
struct ExtensionManifest {
    std::string name;
    std::string entry;
    std::string install;
};

// Pure -- parses `key = "value"` lines (name/entry/install, same subset
// util::parse_config uses). Returns nullopt if `name` or `entry` is missing
// -- both are required to route a request.
[[nodiscard]] std::optional<ExtensionManifest> parse_extension_manifest(std::string_view toml);

// A parsed CGI-style response: status line + headers + blank line + body,
// same shape traditional CGI scripts (and this project's extension
// gateway) speak.
struct CgiResponse {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

// Pure -- parses "Status: NNN\nHeader: value\n...\n\n<body>". A missing or
// malformed Status line defaults to 200 (matches classic CGI: Status is
// optional, absence means OK). Everything after the first blank line is
// the body verbatim, unparsed.
[[nodiscard]] CgiResponse parse_cgi_response(std::string_view raw);

// Pure -- builds the request tvshow writes to the extension's stdin:
// "METHOD <method>\nPATH <path>\nQUERY <query>\n\n<body>". `body` is
// written verbatim after the blank line (no length prefix needed --
// stdin is closed after writing, so EOF marks the end).
[[nodiscard]] std::string build_cgi_request(std::string_view method, std::string_view path,
                                            std::string_view query, std::string_view body);

}  // namespace tvshow::net
