#include "server_fixture.hpp"

#include <poll.h>
#include <sys/types.h>  // NOLINT(misc-include-cleaner) -- provides ssize_t
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tvshow::itest {

namespace {

constexpr int kStartupTimeoutMs = 5000;

// Reads from fd until a newline or timeout, returning the line (without
// the newline). Throws if the server doesn't announce readiness in time.
std::string read_line_with_timeout(int fd) {
    std::string line;
    std::array<char, 1> byte{};
    while (true) {
        pollfd pfd{fd, POLLIN, 0};                        // NOLINT(misc-include-cleaner)
        const int rc = poll(&pfd, 1, kStartupTimeoutMs);  // NOLINT(misc-include-cleaner)
        if (rc <= 0) {
            throw std::runtime_error("tvshow-srv did not announce readiness in time");
        }
        const ssize_t n = read(fd, byte.data(), 1);
        if (n <= 0) {
            throw std::runtime_error("tvshow-srv closed its output before announcing readiness");
        }
        if (byte[0] == '\n') {
            return line;
        }
        line += byte[0];
    }
}

// Parses the port out of "tvshow-srv: listening on http://127.0.0.1:PORT/ ...".
std::string parse_base_url(const std::string& line) {
    constexpr std::string_view k_marker = "http://127.0.0.1:";
    const auto pos = line.find(k_marker);
    if (pos == std::string::npos) {
        throw std::runtime_error("could not find listen address in: " + line);
    }
    auto start = pos + k_marker.size();
    auto end = line.find_first_of("/ ", start);
    if (end == std::string::npos) {
        end = line.size();
    }
    return "http://127.0.0.1:" + line.substr(start, end - start);
}

}  // namespace

ServerFixture::ServerFixture() {
    std::array<int, 2> pipe_fds{-1, -1};
    if (pipe(pipe_fds.data()) != 0) {
        throw std::runtime_error("pipe() failed");
    }

    const int pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[1]);
        execl(TVSHOW_SRV_PATH, TVSHOW_SRV_PATH, "--port", "0", static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipe_fds[1]);
    pid_ = pid;
    try {
        const std::string line = read_line_with_timeout(pipe_fds[0]);
        base_url_ = parse_base_url(line);
    } catch (...) {
        close(pipe_fds[0]);
        kill(pid_, SIGTERM);  // NOLINT(misc-include-cleaner)
        waitpid(pid_, nullptr, 0);
        throw;
    }
    close(pipe_fds[0]);
}

ServerFixture::~ServerFixture() {
    if (pid_ > 0) {
        kill(pid_, SIGTERM);  // NOLINT(misc-include-cleaner)
        waitpid(pid_, nullptr, 0);
    }
}

}  // namespace tvshow::itest
