#pragma once

#include <string>
#include <string_view>
#include <variant>

namespace tvshow::app {

// adr-extension-ui-protocol: structured widget commands a window-provider
// child can print (one per stdout line) once it has opted in by printing
// UI_INIT as its very first line. Children that never print UI_INIT keep
// today's plain-scrollback behavior (adr-external-window-provider) --
// this is additive, not a breaking protocol change.

struct UiInit {};

struct UiButton {
    int id = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    std::string label;
};

struct UiText {
    int id = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 1;
    std::string value;  // "\n" escapes already unescaped to real newlines
};

struct UiClear {};

struct UiTitle {
    std::string value;
};

// Unrecognized command word, or a recognized command missing required
// fields -- ignored by the caller, same degrade-gracefully stance as
// net::parse_blocklist's unknown-prefix handling.
struct UiIgnored {};

using UiCommand = std::variant<UiIgnored, UiInit, UiButton, UiText, UiClear, UiTitle>;

// Parses one protocol line (no trailing newline expected either way).
[[nodiscard]] UiCommand parse_ui_command(std::string_view line);

// "CLICK id=<id>" -- the only tvshow -> child message.
[[nodiscard]] std::string format_click_event(int id);

}  // namespace tvshow::app
