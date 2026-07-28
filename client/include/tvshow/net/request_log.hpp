#pragma once

#include <deque>
#include <string>

namespace tvshow::net {

struct RequestLogEntry {
    std::string method;
    std::string url;
    int status = 0;  // 0 = network error, no HTTP status received
    std::size_t bytes = 0;
    long long elapsed_ms = 0;
};

// Ring buffer of recent HTTP requests (adr-dev-tools), newest-first on read.
// Capped so a long session doesn't grow this unbounded.
class RequestLog {
public:
    static constexpr std::size_t kCapacity = 50;

    void record(RequestLogEntry entry) {
        entries_.push_front(std::move(entry));
        while (entries_.size() > kCapacity) { entries_.pop_back(); }
    }

    [[nodiscard]] const std::deque<RequestLogEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    void clear() noexcept { entries_.clear(); }

private:
    std::deque<RequestLogEntry> entries_;
};

// One human-readable line for `entry`, e.g. "GET 200 1234B 42ms http://...".
[[nodiscard]] std::string format_request_log_entry(const RequestLogEntry& entry);

}  // namespace tvshow::net
