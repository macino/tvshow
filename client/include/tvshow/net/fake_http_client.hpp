#pragma once

#include "tvshow/net/http_client.hpp"
#include "tvshow/util/url.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tvshow::net {

// In-memory HTTP client for unit and integration tests.
// Routes are registered as exact-URL-string→handler pairs.
class FakeHttpClient : public HttpClient {
public:
    using Handler = std::function<Result(const util::Url&)>;

    // Register a handler for a URL (matched by to_string()).
    void on(std::string url, Handler handler);

    // Register a fixed Response for a URL.
    void on(std::string url, Response resp);

    [[nodiscard]] Result get(const util::Url& url, const Headers& extra_headers = {},
                             int max_redirects = 5) override;
    [[nodiscard]] Result post(const util::Url& url, std::string_view body,
                              const Headers& extra_headers = {}, int max_redirects = 5,
                              std::string_view content_type =
                                  "application/x-www-form-urlencoded") override;

private:
    std::unordered_map<std::string, Handler> routes_;

    Result dispatch(const util::Url& url, int max_redirects);
};

}  // namespace tvshow::net
