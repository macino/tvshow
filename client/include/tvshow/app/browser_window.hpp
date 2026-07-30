#pragma once

#include "tvshow/app/browser_view.hpp"
#include "tvshow/app/page.hpp"

#define Uses_TInputLine
#define Uses_TScrollBar
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::app {

enum class AddressBarMode : std::uint8_t { Modal, Persistent };

// BrowserWindow hosts BrowserView inside a TWindow.
//
// Modal mode (default): plain window with no permanent address bar.
//   Ctrl-L is handled by Application and opens an inputBox dialog.
//
// Persistent mode (--address-bar=persistent): a one-row TInputLine sits at
//   the top of the window; Enter in it validates the URL and navigates.
//   Content viewport is one row shorter.
class BrowserWindow : public TWindow {
public:
    BrowserWindow(const TRect& bounds, AddressBarMode mode, Page page,
                  SharedBrowsingState* shared = nullptr,
                  ForcedStyle initial_style = ForcedStyle::Auto);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;
    TPalette& getPalette() const override;

    // Select-all and focus the address bar (persistent mode only; no-op in modal).
    void focus_address_bar();

    // Navigate the inner BrowserView to url (validates first; no-op on bad URL).
    void navigate(std::string_view url);
    void navigate_back();
    void navigate_forward();
    void reload();

    // Called from Application::idle() to animate spinner or apply a completed load.
    void tick_if_loading();

    // Re-render this window using its own current forced style.
    void apply_forced_style() { view_->apply_forced_style(); }
    // Sets this window's own forced style (View > Style acts on the active
    // window only, not every open tab).
    void set_forced_style(ForcedStyle fs) { view_->set_forced_style(fs); }
    [[nodiscard]] ForcedStyle forced_style() const { return view_->forced_style(); }

    // Returns the URL of the currently displayed page.
    [[nodiscard]] std::string_view current_url() const { return view_->page().url; }

private:
    AddressBarMode mode_;
    BrowserView* view_;             // non-owning pointer into child list
    TInputLine* bar_{nullptr};      // non-owning; null in Modal mode
    TScrollBar* vscroll_{nullptr};  // non-owning; null until constructed

    void reposition(const TRect& inner);

    // adr-sandboxed-scripting: a served page can request a size/color
    // closer to a native TWindow's look via `<meta name="tvshow-window-
    // size" content="WxH">` / `<meta name="tvshow-window-color" content=
    // "gray|cyan|blue">` -- read once per page (tracked via
    // view_->page_generation()), applies to any page, not just extension
    // URLs (cosmetic-only, no security boundary crossed).
    enum class PaletteHint : std::uint8_t { None, Gray, Cyan, Blue };
    PaletteHint palette_hint_ = PaletteHint::None;
    int last_page_generation_ = -1;
    void apply_page_window_hints();

    // URL-bar Tab-completion state.
    std::string completion_prefix_;
    std::string completion_current_;
    std::vector<std::string> completion_candidates_;
    std::size_t completion_idx_ = 0;
    bool completion_valid_ = false;

    void handle_url_completion(bool reverse);
};

}  // namespace tvshow::app
