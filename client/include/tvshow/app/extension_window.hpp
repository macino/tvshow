#pragma once

#include "tvshow/app/extension_process.hpp"

#define Uses_TInputLine
#define Uses_TWindow
#include <tvision/tv.h>

#include <memory>
#include <string>
#include <vector>

namespace tvshow::app {

// adr-external-window-provider: hosts one embedded 3rd-party extension
// (calculator, translator, ...) — spawns `argv`, wires an input line to the
// child's stdin, and appends whatever the child writes to stdout as
// scrollback lines. tvshow owns the window chrome; the extension only ever
// speaks plain text over the pipe, never draws into tvshow's CharGrid
// directly.
class ExtensionWindow : public TWindow {
public:
    ExtensionWindow(const TRect& bounds, const char* win_title, std::vector<std::string> argv);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    // Called from Application::idle(): pulls any new output from the child
    // and appends it to scrollback. No-op if nothing new.
    void poll();

private:
    std::unique_ptr<ExtensionProcess> process_;
    TInputLine* input_{nullptr};
    class ScrollbackViewer* viewer_{nullptr};  // non-owning; owned by TGroup
    std::vector<std::string> lines_;
    bool exited_notice_shown_ = false;

    void append_line(std::string line);
    void reposition(const TRect& inner);
};

}  // namespace tvshow::app
