#pragma once

#include "tvshow/app/extension_process.hpp"
#include "tvshow/app/label_view.hpp"

#define Uses_TInputLine
#define Uses_TButton
#define Uses_TWindow
#include <tvision/tv.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tvshow::app {

// adr-external-window-provider + adr-extension-ui-protocol: hosts one
// embedded 3rd-party extension (calculator, calendar, ...). Two modes,
// selected by the child's first output line:
//
//   - Plain-scrollback mode (default, unchanged since adr-external-window-
//     provider): an input line feeds the child's stdin one line per Enter;
//     whatever the child writes to stdout is appended to a scrollback list.
//   - Structured UI mode (child's first line is exactly "UI_INIT"): the
//     child instead describes BUTTON/TEXT widgets (extension_ui_protocol.hpp)
//     that tvshow renders as real TButton/LabelView views; button presses
//     are sent back as "CLICK id=<n>" lines. No input line, no scrollback --
//     all interaction is clicks.
//
// Either way tvshow owns the window chrome; the extension never draws into
// tvshow's CharGrid directly.
class ExtensionWindow : public TWindow {
public:
    ExtensionWindow(const TRect& bounds, const char* win_title, std::vector<std::string> argv);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    // Called from Application::idle(): pulls any new output from the child
    // and applies it (scrollback append, or UI-protocol commands). No-op if
    // nothing new.
    void poll();

private:
    std::unique_ptr<ExtensionProcess> process_;
    bool ui_mode_ = false;
    bool first_line_seen_ = false;
    std::string pending_line_;  // partial line carried over between poll() calls

    // Plain-scrollback mode state (unused once ui_mode_ is true).
    TInputLine* input_{nullptr};
    class ScrollbackViewer* viewer_{nullptr};  // non-owning; owned by TGroup
    std::vector<std::string> lines_;

    // Structured UI mode state.
    std::unordered_map<int, TButton*> buttons_;    // non-owning; owned by TGroup
    std::unordered_map<int, LabelView*> texts_;    // non-owning; owned by TGroup

    bool exited_notice_shown_ = false;
    std::string title_storage_;  // backs TWindow::title when a TITLE command reassigns it

    void append_line(std::string line);
    void handle_protocol_line(std::string_view line);
    void clear_ui_widgets();
    void reposition(const TRect& inner);
};

}  // namespace tvshow::app
