#include "tvshow/net/http_client.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace tvshow::net {

namespace {

// Extract the value of a Content-Type header field parameter.
// E.g. content_type_param("text/html; charset=utf-8", "charset") → "utf-8"
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string content_type_param(std::string_view ct, std::string_view key) {
    // Skip past the media type.
    auto pos = ct.find(';');
    if (pos == std::string_view::npos)
        return {};

    while (pos != std::string_view::npos) {
        const auto next = ct.find(';', pos + 1);
        const auto param =
            ct.substr(pos + 1, next == std::string_view::npos ? next : next - pos - 1);

        // Trim leading whitespace.
        const auto start = param.find_first_not_of(' ');
        if (start == std::string_view::npos) {
            pos = next;
            continue;
        }
        const auto trimmed = param.substr(start);

        // Check key match (case-insensitive).
        if (trimmed.size() > key.size() + 1 && trimmed.at(key.size()) == '=') {
            std::string lower_key(trimmed.substr(0, key.size()));
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::string target_key(key);
            std::transform(target_key.begin(), target_key.end(), target_key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower_key == target_key) {
                return std::string(trimmed.substr(key.size() + 1));
            }
        }
        pos = next;
    }
    return {};
}

}  // namespace

std::string Response::content_type() const {
    const auto it = headers.find("content-type");
    if (it == headers.end())
        return {};
    const auto ct = std::string_view(it->second);
    const auto semi = ct.find(';');
    return std::string(semi == std::string_view::npos ? ct : ct.substr(0, semi));
}

std::string Response::charset() const {
    const auto it = headers.find("content-type");
    if (it == headers.end())
        return "utf-8";
    auto cs = content_type_param(it->second, "charset");
    if (!cs.empty())
        return cs;
    return "utf-8";
}

}  // namespace tvshow::net
