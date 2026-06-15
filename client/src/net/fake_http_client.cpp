#include "tvshow/net/fake_http_client.hpp"

#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace tvshow::net {

void FakeHttpClient::on(std::string url, Handler handler) {
    routes_.emplace(std::move(url), std::move(handler));
}

void FakeHttpClient::on(std::string url, Response resp) {
    routes_.emplace(std::move(url),
                    [r = std::move(resp)](const util::Url&) -> Result { return r; });
}

Result FakeHttpClient::get(const util::Url& url, int max_redirects) {
    return dispatch(url, max_redirects);
}

Result FakeHttpClient::post(const util::Url& url, std::string_view /*body*/, int max_redirects) {
    return dispatch(url, max_redirects);
}

Result FakeHttpClient::dispatch(const util::Url& url, int max_redirects) {
    const std::string key = url.to_string();
    const auto it = routes_.find(key);
    if (it == routes_.end()) {
        return NetworkError{"no route for: " + key};
    }

    Result result = it->second(url);

    // Follow redirects.
    for (int hop = 0; hop < max_redirects; ++hop) {
        const auto* resp = std::get_if<Response>(&result);
        if (resp == nullptr)
            break;
        const int status = resp->status;
        if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)
            break;

        const auto loc_it = resp->headers.find("location");
        if (loc_it == resp->headers.end())
            break;

        const auto next_url = url.resolve(loc_it->second);
        if (!next_url)
            break;

        const auto next_it = routes_.find(next_url->to_string());
        if (next_it == routes_.end()) {
            return NetworkError{"no route for redirect target: " + next_url->to_string()};
        }
        result = next_it->second(*next_url);
    }

    return result;
}

}  // namespace tvshow::net
