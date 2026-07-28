#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>

namespace tvshow::app {

// adr-external-window-provider: spawns `argv` with its stdin/stdout wired to
// pipes tvshow owns. No ABI, no dlopen -- the extension is an arbitrary
// executable in any language speaking plain lines over stdin/stdout.
// Crash-isolated by construction: a bad extension can't corrupt tvshow's own
// process, only its own pipe ends (EOF/closed fd), handled the same as a
// clean exit.
class ExtensionProcess {
public:
    explicit ExtensionProcess(std::vector<std::string> argv);
    ~ExtensionProcess();

    ExtensionProcess(const ExtensionProcess&) = delete;
    ExtensionProcess& operator=(const ExtensionProcess&) = delete;
    ExtensionProcess(ExtensionProcess&&) = delete;
    ExtensionProcess& operator=(ExtensionProcess&&) = delete;

    // True if the spawn succeeded and the child hasn't exited (checked
    // lazily via a non-blocking waitpid on each call).
    [[nodiscard]] bool alive();

    // Appends `text` + '\n' to the child's stdin. No-op if not alive.
    void write_line(std::string_view text);

    // Non-blocking read of whatever the child has written to stdout since
    // the last call. Returns "" if nothing new (or the child isn't alive).
    [[nodiscard]] std::string read_available();

private:
    pid_t pid_ = -1;
    int stdin_fd_ = -1;   // write end, parent -> child
    int stdout_fd_ = -1;  // read end, child -> parent
    bool exited_ = false;
};

}  // namespace tvshow::app
