#pragma once

#define Uses_TView
#include "tvshow/app/page.hpp"
#include "tvshow/layout/form_focus.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"

#include <tvision/tv.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::app {

// Hosts one loaded Page: renders it, tracks the focused link or form control
// (SPEC §12.1 / §13.2 Tab/Shift-Tab cycling), and resolves/navigates
// Enter-on-link, maintaining a simple per-window history stack.
class BrowserView : public TView {
public:
    BrowserView(const TRect& bounds, Page page);

    void draw() override;
    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;

    // Navigate to url, pushing onto history. No-op on load failure.
    void navigate_to(std::string_view url);

    [[nodiscard]] const Page& page() const { return page_; }

private:
    Page page_;
    std::vector<layout::Link> links_;
    std::vector<layout::FormFocus> form_controls_;
    render::FormValues form_values_;
    int focused_ = -1;  // index into links_ + form_controls_ (0..n-1), or -1

    std::vector<std::string> history_;
    size_t history_pos_ = 0;

    [[nodiscard]] int total_focusables() const;
    [[nodiscard]] bool is_link_focused() const;
    [[nodiscard]] const layout::FormFocus* focused_fc() const;

    void relayout();
    [[nodiscard]] render::CharGrid render_grid() const;
    void navigate(const std::string& url, bool push_history);
    void focus_next(int direction);
    void handle_form_input(unsigned keyCode);
};

}  // namespace tvshow::app
