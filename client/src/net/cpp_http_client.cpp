#include "tvshow/net/cpp_http_client.hpp"

#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <httplib.h>

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

[[nodiscard]] Result to_result(const httplib::Result& res) {
    if (!res) {
        return NetworkError{httplib::to_string(res.error())};
    }
    Response out;
    out.status = res->status;
    for (const auto& [key, value] : res->headers) {
        out.headers[key] = value;
    }
    out.body = res->body;
    return out;
}

[[nodiscard]] bool is_redirect_status(int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

// Follows redirects with a plain loop (rather than recursion) since a
// redirect can change host, requiring a fresh httplib::Client per hop.
// GET-on-redirect is used throughout: correct for 303 by spec, and matches
// how browsers treat 301/302/307/308 in practice after the initial GET.
[[nodiscard]] Result get_following_redirects(util::Url url, int max_redirects) {
    for (int hop = 0;; ++hop) {
        if (url.scheme() != "http") {
            return NetworkError{"only http:// is supported (no TLS compiled in): " +
                                url.to_string()};
        }
        httplib::Client cli(url.host(), url.effective_port());
        Result result = to_result(cli.Get(request_target(url)));

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

Result CppHttpClient::get(const util::Url& url, int max_redirects) {
    return get_following_redirects(url, max_redirects);
}

Result CppHttpClient::post(const util::Url& url, std::string_view body, int max_redirects) {
    if (url.scheme() != "http") {
        return NetworkError{"only http:// is supported (no TLS compiled in): " + url.to_string()};
    }
    httplib::Client cli(url.host(), url.effective_port());
    Result result = to_result(
        cli.Post(request_target(url), std::string(body), "application/x-www-form-urlencoded"));

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
    return get_following_redirects(*next_url, max_redirects - 1);
}

}  // namespace tvshow::net
