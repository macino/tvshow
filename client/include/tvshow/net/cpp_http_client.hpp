#pragma once

#include "tvshow/net/http_client.hpp"

namespace tvshow::net {

// Real HTTP/1.1 client backed by cpp-httplib. http:// only — no TLS support
// is compiled in (see docs/decisions/001), so https:// URLs fail fast with
// a NetworkError rather than silently downgrading or hanging.
class CppHttpClient : public HttpClient {
public:
    [[nodiscard]] Result get(const util::Url& url, int max_redirects = 5) override;
    [[nodiscard]] Result post(const util::Url& url, std::string_view body,
                              int max_redirects = 5) override;
};

}  // namespace tvshow::net
