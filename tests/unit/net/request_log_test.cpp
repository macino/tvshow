#include "tvshow/net/request_log.hpp"

#include <doctest/doctest.h>

using tvshow::net::RequestLog;
using tvshow::net::RequestLogEntry;
using tvshow::net::format_request_log_entry;

TEST_CASE("format_request_log_entry: successful request") {
    RequestLogEntry e{"GET", "http://example.com/", 200, 1234, 42};
    const auto s = format_request_log_entry(e);
    CHECK(s.find("GET") != std::string::npos);
    CHECK(s.find("200") != std::string::npos);
    CHECK(s.find("1234B") != std::string::npos);
    CHECK(s.find("42ms") != std::string::npos);
    CHECK(s.find("http://example.com/") != std::string::npos);
}

TEST_CASE("format_request_log_entry: network error (status 0) shows ERR") {
    RequestLogEntry e{"GET", "http://down.example.com/", 0, 0, 5};
    const auto s = format_request_log_entry(e);
    CHECK(s.find("ERR") != std::string::npos);
}

TEST_CASE("RequestLog: newest entry first") {
    RequestLog log;
    log.record({"GET", "http://a", 200, 1, 1});
    log.record({"GET", "http://b", 200, 1, 1});
    REQUIRE(log.entries().size() == 2);
    CHECK(log.entries().front().url == "http://b");
    CHECK(log.entries().back().url == "http://a");
}

TEST_CASE("RequestLog: caps at kCapacity, dropping oldest") {
    RequestLog log;
    for (std::size_t i = 0; i < RequestLog::kCapacity + 10; ++i) {
        log.record({"GET", "http://" + std::to_string(i), 200, 1, 1});
    }
    CHECK(log.entries().size() == RequestLog::kCapacity);
    // Newest recorded is at the front.
    CHECK(log.entries().front().url == "http://" + std::to_string(RequestLog::kCapacity + 9));
}

TEST_CASE("RequestLog: clear empties the log") {
    RequestLog log;
    log.record({"GET", "http://a", 200, 1, 1});
    log.clear();
    CHECK(log.empty());
}
