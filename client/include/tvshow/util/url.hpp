#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tvshow::util {

// Parsed representation of an HTTP/HTTPS URL (RFC 3986 subset).
class Url {
public:
    // Parse an absolute URL. Returns nullopt if the string is not a valid
    // http:// or https:// URL.
    [[nodiscard]] static std::optional<Url> parse(std::string_view s);

    // Resolve a possibly-relative reference against this base URL (RFC 3986 §5.2).
    // Returns nullopt if the reference cannot be resolved.
    [[nodiscard]] std::optional<Url> resolve(std::string_view ref) const;

    // Serialize back to a URL string.
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] const std::string& scheme() const noexcept { return scheme_; }
    [[nodiscard]] const std::string& host() const noexcept { return host_; }

    // Explicit port number from the URL, or 0 if not specified.
    [[nodiscard]] uint16_t port() const noexcept { return port_; }

    // Effective port: specified port, or the default for the scheme (80/443).
    [[nodiscard]] uint16_t effective_port() const noexcept;

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& query() const noexcept { return query_; }
    [[nodiscard]] const std::string& fragment() const noexcept { return fragment_; }

    [[nodiscard]] bool operator==(const Url&) const noexcept = default;

private:
    std::string scheme_;
    std::string host_;
    uint16_t port_ = 0;
    std::string path_;
    std::string query_;
    std::string fragment_;
};

// Percent-decode a URL-encoded string (+ decoded as space).
[[nodiscard]] std::string percent_decode(std::string_view s);

// Percent-encode a string; unreserved chars (RFC 3986 §2.3) pass through.
[[nodiscard]] std::string percent_encode(std::string_view s);

// Resolves a possibly-relative href against a base "file://" URL by simple
// path-joining (no ".." normalization — the filesystem handles that when
// the path is opened). `href` already carrying its own scheme, or starting
// with '/', is returned unchanged (modulo the file:// prefix); an empty
// href returns `base`. Used for M11 link navigation, where the only
// transport is local files (HTTP lands in M12).
[[nodiscard]] std::string resolve_file_url(std::string_view base, std::string_view href);

}  // namespace tvshow::util
