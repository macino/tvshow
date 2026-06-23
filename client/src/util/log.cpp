#include "tvshow/util/log.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace tvshow::util::log {

namespace {

struct State {
    Level min_level = Level::Warn;
    FILE* file = nullptr;          // null = not initialised (use stderr)
    bool use_stderr_only = false;  // set when path == "-"
    std::mutex mu;
};

State& state() {
    static State s;
    return s;
}

// Make every directory component in path (like `mkdir -p`).
void mkdir_p(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '/') {
            const std::string partial = path.substr(0, i);
            ::mkdir(partial.c_str(), S_IRWXU);
        }
    }
    ::mkdir(path.c_str(), S_IRWXU);
}

std::string default_log_path() {
    const char* home = ::getenv("HOME");
    if (!home || home[0] == '\0') {
        home = "/tmp";
    }
    return std::string(home) + "/.cache/tvshow/log";
}

const char* level_tag(Level l) noexcept {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?????";
}

void write_line(Level l, std::string_view msg) {
    auto& s = state();

    // Timestamp.
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
    ::gmtime_r(&t, &tm_buf);
    std::array<char, 20> ts{};
    std::strftime(ts.data(), ts.size(), "%Y-%m-%d %H:%M:%S", &tm_buf);

    const char* tag = level_tag(l);

    if (s.use_stderr_only || s.file == nullptr) {
        std::fprintf(stderr, "[%s] %s %.*s\n", ts.data(), tag,
                     static_cast<int>(msg.size()), msg.data());
        return;
    }
    std::fprintf(s.file, "[%s] %s %.*s\n", ts.data(), tag,
                 static_cast<int>(msg.size()), msg.data());
    std::fflush(s.file);
}

}  // namespace

Level parse_level(std::string_view s) noexcept {
    if (s == "debug") { return Level::Debug; }
    if (s == "info") { return Level::Info; }
    if (s == "warn" || s == "warning") { return Level::Warn; }
    if (s == "error") { return Level::Error; }
    return Level::Warn;
}

void init(Level min_level, std::string_view path) {
    auto& s = state();
    std::lock_guard lock(s.mu);

    if (s.file) {
        std::fclose(s.file);
        s.file = nullptr;
    }

    s.min_level = min_level;

    if (path == "-") {
        s.use_stderr_only = true;
        return;
    }

    const std::string log_path = path.empty() ? default_log_path() : std::string(path);

    // Create parent directory.
    const auto last_slash = log_path.rfind('/');
    if (last_slash != std::string::npos) {
        mkdir_p(log_path.substr(0, last_slash));
    }

    s.file = std::fopen(log_path.c_str(), "ae");  // append, close-on-exec
    // Silently fall back to stderr if the file can't be opened.
}

void debug(std::string_view msg) {
    auto& s = state();
    if (s.min_level > Level::Debug) { return; }
    std::lock_guard lock(s.mu);
    write_line(Level::Debug, msg);
}

void info(std::string_view msg) {
    auto& s = state();
    if (s.min_level > Level::Info) { return; }
    std::lock_guard lock(s.mu);
    write_line(Level::Info, msg);
}

void warn(std::string_view msg) {
    auto& s = state();
    if (s.min_level > Level::Warn) { return; }
    std::lock_guard lock(s.mu);
    write_line(Level::Warn, msg);
}

void error(std::string_view msg) {
    auto& s = state();
    if (s.min_level > Level::Error) { return; }
    std::lock_guard lock(s.mu);
    write_line(Level::Error, msg);
}

}  // namespace tvshow::util::log
