#pragma once

#include "tvshow/util/url.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace tvshow::net {

// Case-insensitive HTTP header map (lowercase keys normalised on insert).
using Headers = std::unordered_map<std::string, std::string>;

struct Response {
    int status = 0;
    Headers headers;
    std::string body;

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
    [[nodiscard]] virtual Result get(const util::Url& url, int max_redirects = 5) = 0;

    // Perform a POST request with application/x-www-form-urlencoded body.
    [[nodiscard]] virtual Result post(const util::Url& url, std::string_view body,
                                      int max_redirects = 5) = 0;
};

}  // namespace tvshow::net
