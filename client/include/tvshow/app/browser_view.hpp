#pragma once

#define Uses_TView
#include "tvshow/app/page.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/render/chargrid.hpp"

#include <tvision/tv.h>

#include <string>
#include <vector>

namespace tvshow::app {

// Hosts one loaded Page: renders it, tracks the focused link (SPEC §12.1
// Tab/Shift-Tab cycling), and resolves/navigates Enter-on-link, maintaining
// a simple per-window history stack for Alt-Left/Alt-Right. Still
// file://-only (HTTP fetch lands in M12) and still a plain TView (the full
// TBrowserWindow : TWindow refactor in SPEC §11.3 lands in M16).
class BrowserView : public TView {
public:
    BrowserView(const TRect& bounds, Page page);

    void draw() override;
    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

private:
    Page page_;
    std::vector<layout::Link> links_;
    int focused_ = -1;  // index into links_, or -1 if nothing focused

    std::vector<std::string> history_;
    size_t history_pos_ = 0;

    void relayout();
    [[nodiscard]] render::CharGrid render_grid() const;
    void navigate(const std::string& url, bool push_history);
    void focus_next(int direction);
};

}  // namespace tvshow::app
