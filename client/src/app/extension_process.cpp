#include "tvshow/app/extension_process.hpp"

#include <array>
#include <csignal>
#include <cstdio>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tvshow::app {

namespace {

void set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

}  // namespace

ExtensionProcess::ExtensionProcess(std::vector<std::string> argv) {
    if (argv.empty()) {
        return;
    }

    int in_pipe[2] = {-1, -1};   // parent writes in_pipe[1] -> child reads in_pipe[0] (its stdin)
    int out_pipe[2] = {-1, -1};  // child writes out_pipe[1] (its stdout) -> parent reads out_pipe[0]
    if (::pipe(in_pipe) != 0 || ::pipe(out_pipe) != 0) {
        return;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(in_pipe[0]);
        ::close(in_pipe[1]);
        ::close(out_pipe[0]);
        ::close(out_pipe[1]);
        return;
    }

    if (pid == 0) {
        ::dup2(in_pipe[0], STDIN_FILENO);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::close(in_pipe[0]);
        ::close(in_pipe[1]);
        ::close(out_pipe[0]);
        ::close(out_pipe[1]);

        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (auto& s : argv) { cargv.push_back(s.data()); }
        cargv.push_back(nullptr);
        ::execvp(cargv[0], cargv.data());
        ::_exit(127);  // exec failed
    }

    // Parent: close the ends we don't use, keep the other two.
    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    pid_ = pid;
    stdin_fd_ = in_pipe[1];
    stdout_fd_ = out_pipe[0];
    set_nonblocking(stdout_fd_);
}

ExtensionProcess::~ExtensionProcess() {
    if (pid_ > 0 && !exited_) {
        ::kill(pid_, SIGTERM);
        int status = 0;
        ::waitpid(pid_, &status, WNOHANG);  // best-effort reap, don't block teardown
    }
    if (stdin_fd_ >= 0) { ::close(stdin_fd_); }
    if (stdout_fd_ >= 0) { ::close(stdout_fd_); }
}

bool ExtensionProcess::alive() {
    if (pid_ <= 0 || exited_) {
        return false;
    }
    int status = 0;
    const pid_t res = ::waitpid(pid_, &status, WNOHANG);
    if (res == pid_) {
        exited_ = true;
        return false;
    }
    return true;
}

void ExtensionProcess::write_line(std::string_view text) {
    if (!alive() || stdin_fd_ < 0) {
        return;
    }
    std::string line(text);
    line += '\n';
    // Best-effort: a short write or EPIPE just means the extension stopped
    // reading -- degrade gracefully, same stance as the rest of net/.
    (void)::write(stdin_fd_, line.data(), line.size());
}

void ExtensionProcess::write(std::string_view text) {
    if (!alive() || stdin_fd_ < 0) {
        return;
    }
    (void)::write(stdin_fd_, text.data(), text.size());
}

void ExtensionProcess::close_stdin() {
    if (stdin_fd_ >= 0) {
        ::close(stdin_fd_);
        stdin_fd_ = -1;
    }
}

std::string ExtensionProcess::read_available() {
    if (pid_ <= 0 || stdout_fd_ < 0) {
        return {};
    }
    std::string out;
    std::array<char, 4096> buf{};
    while (true) {
        const ssize_t n = ::read(stdout_fd_, buf.data(), buf.size());
        if (n > 0) {
            out.append(buf.data(), static_cast<size_t>(n));
            continue;
        }
        break;  // n == 0 (EOF) or n < 0 (EAGAIN/EWOULDBLOCK -- nothing more right now)
    }
    return out;
}

}  // namespace tvshow::app
