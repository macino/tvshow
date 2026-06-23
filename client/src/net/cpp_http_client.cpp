#include "tvshow/net/cpp_http_client.hpp"

#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <httplib.h>

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace tvshow::net {

namespace {

[[nodiscard]] std::string request_target(const util::Url& url) {
    std::string target = url.path().empty() ? "/" : url.path();
    if (!url.query().empty()) {
        target += "?";
        target += url.query();
    }
    return target;
}

// Build the base URL string ("scheme://host:port") understood by the
// httplib::Client universal constructor, which selects SSLClient for https.
[[nodiscard]] std::string client_base(const util::Url& url) {
    return url.scheme() + "://" + url.host() + ":" + std::to_string(url.effective_port());
}

[[nodiscard]] Result to_result(const httplib::Result& res) {
    if (!res) {
        return NetworkError{httplib::to_string(res.error())};
    }
    Response out;
    out.status = res->status;
    for (const auto& [key, value] : res->headers) {
        std::string lower_key = key;
        for (char& c : lower_key) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
        if (lower_key == "set-cookie") {
            out.set_cookies.push_back(value);
        } else {
            out.headers[std::move(lower_key)] = value;
        }
    }
    out.body = res->body;
    return out;
}

[[nodiscard]] bool is_redirect_status(int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

// Build an httplib::Headers from our Headers map.
[[nodiscard]] httplib::Headers to_httplib_headers(const Headers& h) {
    httplib::Headers out;
    for (const auto& [k, v] : h) {
        out.emplace(k, v);
    }
    return out;
}

// Follows redirects with a plain loop (rather than recursion) since a
// redirect can change host or scheme, requiring a fresh Client per hop.
// GET-on-redirect is used throughout: correct for 303 by spec, and matches
// how browsers treat 301/302/307/308 in practice after the initial GET.
// extra_headers (e.g., Cookie) are forwarded on every hop.
[[nodiscard]] Result get_following_redirects(util::Url url, const Headers& extra_headers,
                                             int max_redirects) {
    const httplib::Headers hlib_extra = to_httplib_headers(extra_headers);
    for (int hop = 0;; ++hop) {
        httplib::Client cli(client_base(url));
        Result result = to_result(cli.Get(request_target(url), hlib_extra));

        const auto* resp = std::get_if<Response>(&result);
        if (resp == nullptr || !is_redirect_status(resp->status) || hop >= max_redirects) {
            return result;
        }
        const auto it = resp->headers.find("location");
        if (it == resp->headers.end()) {
            return result;
        }
        std::optional<util::Url> next_url = url.resolve(it->second);
        if (!next_url) {
            return NetworkError{"redirect Location header could not be resolved: " + it->second};
        }
        url = *next_url;
    }
}

}  // namespace

Result CppHttpClient::get(const util::Url& url, const Headers& extra_headers, int max_redirects) {
    return get_following_redirects(url, extra_headers, max_redirects);
}

Result CppHttpClient::post(const util::Url& url, std::string_view body,
                           const Headers& extra_headers, int max_redirects) {
    httplib::Client cli(client_base(url));
    Result result = to_result(
        cli.Post(request_target(url), to_httplib_headers(extra_headers),
                 std::string(body), "application/x-www-form-urlencoded"));

    const auto* resp = std::get_if<Response>(&result);
    if (resp == nullptr || !is_redirect_status(resp->status) || max_redirects <= 0) {
        return result;
    }
    const auto it = resp->headers.find("location");
    if (it == resp->headers.end()) {
        return result;
    }
    std::optional<util::Url> next_url = url.resolve(it->second);
    if (!next_url) {
        return NetworkError{"redirect Location header could not be resolved: " + it->second};
    }
    return get_following_redirects(*next_url, extra_headers, max_redirects - 1);
}

}  // namespace tvshow::net
