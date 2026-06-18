#include "tvshow/app/browser_view.hpp"

#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#include "tvshow/app/page.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/form.hpp"
#include "tvshow/layout/form_data.hpp"
#include "tvshow/layout/form_focus.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/paint/paint.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::app {

BrowserView::BrowserView(const TRect& bounds, Page page) : TView(bounds), page_(std::move(page)) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;
    eventMask |= evKeyDown;
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    history_.push_back(page_.url);
}

int BrowserView::total_focusables() const {
    return static_cast<int>(links_.size() + form_controls_.size());
}

bool BrowserView::is_link_focused() const {
    return focused_ >= 0 && focused_ < static_cast<int>(links_.size());
}

const layout::FormFocus* BrowserView::focused_fc() const {
    const int fc_idx = focused_ - static_cast<int>(links_.size());
    if (fc_idx >= 0 && fc_idx < static_cast<int>(form_controls_.size())) {
        return &form_controls_[static_cast<size_t>(fc_idx)];
    }
    return nullptr;
}

render::CharGrid BrowserView::render_grid() const {
    render::CharGrid grid = render::render(page_.box, form_values_);
    if (is_link_focused()) {
        render::apply_focus(grid, links_[static_cast<size_t>(focused_)].spans);
    } else if (const layout::FormFocus* fc = focused_fc()) {
        render::apply_focus(grid, {fc->span});
    }
    return grid;
}

void BrowserView::draw() {
    TDrawBuffer buf;
    const render::CharGrid grid = render_grid();
    const int rows = std::min(size.y, grid.rows());
    for (int row = 0; row < rows; ++row) {
        paint::draw_row(grid, row, buf);
        writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
    }
}

void BrowserView::changeBounds(const TRect& bounds) {
    TView::changeBounds(bounds);
    relayout();
    drawView();
}

void BrowserView::relayout() {
    page_.box = layout::layout(*page_.tree, {size.x, size.y});
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    if (focused_ >= total_focusables()) {
        focused_ = -1;
    }
}

void BrowserView::navigate_to(std::string_view url) {
    navigate(std::string(url), true);
}

void BrowserView::navigate(const std::string& url, bool push_history) {
    auto page = load_page(url, {size.x, size.y});
    if (!page) {
        return;
    }
    page_ = std::move(*page);
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    form_values_ = {};
    focused_ = -1;
    if (push_history) {
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_pos_) + 1,
                       history_.end());
        history_.push_back(url);
        history_pos_ = history_.size() - 1;
    }
    drawView();
}

void BrowserView::focus_next(int direction) {
    const int n = total_focusables();
    if (n == 0) {
        return;
    }
    if (focused_ < 0) {
        focused_ = direction > 0 ? 0 : n - 1;
    } else {
        focused_ = (focused_ + direction + n) % n;
    }
    drawView();
}

void BrowserView::handle_form_input(unsigned keyCode) {
    const layout::FormFocus* fc = focused_fc();
    if (fc == nullptr) {
        return;
    }
    const unsigned char_code = keyCode & 0xFFU;
    if (fc->kind == layout::FormControlKind::Text ||
        fc->kind == layout::FormControlKind::Password) {
        auto& val = form_values_.text[fc->node];
        if (char_code == 0x08U) {
            if (!val.empty()) {
                val.pop_back();
            }
        } else {
            val += static_cast<char>(char_code);
        }
        drawView();
    } else if (char_code == 0x20U) {
        // Space toggles checkbox / selects radio.
        if (fc->kind == layout::FormControlKind::Checkbox) {
            auto it = form_values_.checked.find(fc->node);
            const bool was = (it != form_values_.checked.end())
                                 ? it->second
                                 : (fc->node->attr("checked").data() != nullptr);
            form_values_.checked[fc->node] = !was;
            drawView();
        } else if (fc->kind == layout::FormControlKind::Radio) {
            form_values_.checked[fc->node] = true;
            drawView();
        }
    }
}

void BrowserView::submit_form() {
    const layout::FormFocus* fc = focused_fc();
    if (fc == nullptr || fc->form == nullptr) {
        return;
    }
    const auto fields =
        layout::collect_form_fields(*fc->form, form_values_.text, form_values_.checked);
    const std::string encoded = layout::encode_fields(fields);

    const std::string_view action_attr = fc->form->attr("action");
    const std::string action_url = util::resolve_url(
        page_.url, action_attr.empty() ? std::string_view(page_.url) : action_attr);

    const std::string_view method_attr = fc->form->attr("method");
    const bool is_post = (method_attr == "post" || method_attr == "POST");

    if (is_post) {
        auto page = post_page(action_url, encoded, {size.x, size.y});
        if (!page) {
            return;
        }
        page_ = std::move(*page);
        links_ = layout::collect_links(page_.box);
        form_controls_ = layout::collect_form_controls(page_.box);
        form_values_ = {};
        focused_ = -1;
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_pos_) + 1,
                       history_.end());
        history_.push_back(action_url);
        history_pos_ = history_.size() - 1;
        drawView();
    } else {
        const std::string get_url = encoded.empty() ? action_url : (action_url + "?" + encoded);
        navigate(get_url, true);
    }
}

void BrowserView::handleEvent(TEvent& event) {
    TView::handleEvent(event);
    if (event.what != evKeyDown) {
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) — TEvent is a tvision union.
    const unsigned keyCode = event.keyDown.keyCode;
    switch (keyCode) {
    case kbTab:
        focus_next(1);
        clearEvent(event);
        break;
    case kbShiftTab:
        focus_next(-1);
        clearEvent(event);
        break;
    case kbEnter:
        if (is_link_focused()) {
            navigate(util::resolve_url(page_.url, links_[static_cast<size_t>(focused_)].href),
                     true);
        } else if (focused_fc() != nullptr) {
            submit_form();
        }
        clearEvent(event);
        break;
    case kbAltLeft:
        navigate_back();
        clearEvent(event);
        break;
    case kbAltRight:
        navigate_forward();
        clearEvent(event);
        break;
    default: {
        const unsigned char_code = keyCode & 0xFFU;
        const bool is_backspace = char_code == 0x08U;
        const bool is_printable = char_code >= 0x20U && char_code < 0x7FU;
        if (focused_fc() != nullptr && (is_backspace || is_printable)) {
            handle_form_input(keyCode);
            clearEvent(event);
        }
        break;
    }
    }
}

void BrowserView::navigate_back() {
    if (history_pos_ > 0) {
        --history_pos_;
        navigate(history_[history_pos_], false);
    }
}

void BrowserView::navigate_forward() {
    if (history_pos_ + 1 < history_.size()) {
        ++history_pos_;
        navigate(history_[history_pos_], false);
    }
}

void BrowserView::reload() {
    navigate(history_[history_pos_], false);
}

}  // namespace tvshow::app
