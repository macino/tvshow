#include "tvshow/app/external_handler.hpp"

#include "tvshow/util/media_kind.hpp"

#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace tvshow::app {

void spawn_handler(std::string_view command_template, std::string_view url) {
    std::vector<std::string> argv = util::build_handler_argv(command_template, url);
    if (argv.empty()) {
        return;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        return;  // fork failed -- degrade gracefully, same as other net/spawn failures
    }
    if (pid == 0) {
        // First child: double-fork so the grandchild (the actual handler) gets
        // reparented to init on exit, avoiding a zombie without the caller
        // needing to track/reap it later.
        const pid_t pid2 = ::fork();
        if (pid2 == 0) {
            std::vector<char*> cargv;
            cargv.reserve(argv.size() + 1);
            for (auto& s : argv) { cargv.push_back(s.data()); }
            cargv.push_back(nullptr);
            ::execvp(cargv[0], cargv.data());
            ::_exit(127);  // exec failed (handler not installed, etc.)
        }
        ::_exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);  // reap the short-lived first child
}

}  // namespace tvshow::app
