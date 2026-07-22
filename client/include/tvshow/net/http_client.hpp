#pragma once

#include "tvshow/util/url.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tvshow::net {

// Case-insensitive HTTP header map (lowercase keys normalised on insert).
using Headers = std::unordered_map<std::string, std::string>;

struct Response {
    int status = 0;
    Headers headers;
    std::string body;
    std::vector<std::string> set_cookies;  // all Set-Cookie header values (multi-valued)

    [[nodiscard]] std::string content_type() const;
    [[nodiscard]] std::string charset() const;
};

struct NetworkError {
    std::string message;
};

// Result of an HTTP request: either a Response or a NetworkError.
using Result = std::variant<Response, NetworkError>;

// Pure HTTP client interface — implemented by CppHttpClient and FakeHttpClient.
class HttpClient {
public:
    HttpClient() = default;
    virtual ~HttpClient() = default;
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = delete;
    HttpClient& operator=(HttpClient&&) = delete;

    // Perform a GET request. Follows redirects (up to max_redirects hops).
    // extra_headers are injected on every hop (e.g., Cookie header).
    [[nodiscard]] virtual Result get(const util::Url& url, const Headers& extra_headers = {},
                                     int max_redirects = 5) = 0;

    // Perform a POST request. content_type defaults to the standard HTML
    // form encoding; pass "multipart/form-data; boundary=..." for file
    // uploads (SPEC Q-28).
    [[nodiscard]] virtual Result post(const util::Url& url, std::string_view body,
                                      const Headers& extra_headers = {}, int max_redirects = 5,
                                      std::string_view content_type =
                                          "application/x-www-form-urlencoded") = 0;
};

}  // namespace tvshow::net
