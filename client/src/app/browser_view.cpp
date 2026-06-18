#include "tvshow/app/browser_view.hpp"

#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TScrollBar
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
    options |= ofSelectable | ofFirstClick;
    eventMask |= evKeyDown | evMouseDown;
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
    const int available = grid.rows() - scroll_row_;
    const int rows = std::min(size.y, available > 0 ? available : 0);
    for (int row = 0; row < rows; ++row) {
        paint::draw_row(grid, scroll_row_ + row, buf);
        writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
    }
}

void BrowserView::changeBounds(const TRect& bounds) {
    TView::changeBounds(bounds);
    relayout();
    drawView();
}

int BrowserView::scroll_limit() const {
    return std::max(0, page_.box.border_box.size.rows - size.y);
}

void BrowserView::sync_vscroll() {
    if (vscroll_ == nullptr) {
        return;
    }
    const int limit = scroll_limit();
    const int pg = std::max(1, size.y - 1);
    vscroll_->setParams(scroll_row_, 0, limit, pg, 1);
}

void BrowserView::scroll_to(int row) {
    scroll_row_ = std::clamp(row, 0, scroll_limit());
    sync_vscroll();
    drawView();
}

void BrowserView::relayout() {
    page_.box = layout::layout(*page_.tree, {size.x, size.y});
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    scroll_row_ = std::clamp(scroll_row_, 0, scroll_limit());
    sync_vscroll();
    if (focused_ >= total_focusables()) {
        focused_ = -1;
    }
}

void BrowserView::navigate_to(std::string_view url) {
    navigate(std::string(url), true);
}

void BrowserView::navigate(const std::string& url, bool push_history) {
    // Strip fragment from URL before loading (fragments are client-side only).
    const auto hash_pos = url.rfind('#');
    const std::string base_url = (hash_pos != std::string::npos) ? url.substr(0, hash_pos) : url;
    const std::string fragment =
        (hash_pos != std::string::npos) ? url.substr(hash_pos + 1) : std::string{};

    auto page = load_page(base_url.empty() ? url : base_url, {size.x, size.y});
    if (!page) {
        return;
    }
    page_ = std::move(*page);
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    form_values_ = {};
    focused_ = -1;
    scroll_row_ = 0;
    if (!fragment.empty()) {
        const int anchor = layout::find_anchor_row(page_.box, fragment);
        if (anchor >= 0) {
            scroll_row_ = std::clamp(anchor, 0, scroll_limit());
        }
    }
    sync_vscroll();
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

    if (event.what == evMouseDown) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const TPoint local = makeLocal(event.mouse.where);
        const tvshow::Point pt{local.x, local.y + scroll_row_};
        for (size_t i = 0; i < links_.size(); ++i) {
            for (const auto& span : links_[i].spans) {
                if (span.contains(pt)) {
                    navigate(util::resolve_url(page_.url, links_[i].href), true);
                    clearEvent(event);
                    return;
                }
            }
        }
        for (size_t i = 0; i < form_controls_.size(); ++i) {
            if (form_controls_[i].span.contains(pt)) {
                focused_ = static_cast<int>(links_.size() + i);
                drawView();
                clearEvent(event);
                return;
            }
        }
        return;
    }

    if (event.what != evKeyDown) {
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) — TEvent is a tvision union.
    const unsigned keyCode = event.keyDown.keyCode;
    switch (keyCode) {
    case kbUp:
        scroll_to(scroll_row_ - 1);
        clearEvent(event);
        break;
    case kbDown:
        scroll_to(scroll_row_ + 1);
        clearEvent(event);
        break;
    case kbPgUp:
        scroll_to(scroll_row_ - std::max(1, size.y - 1));
        clearEvent(event);
        break;
    case kbPgDn:
        scroll_to(scroll_row_ + std::max(1, size.y - 1));
        clearEvent(event);
        break;
    case kbHome:
        scroll_to(0);
        clearEvent(event);
        break;
    case kbEnd:
        scroll_to(scroll_limit());
        clearEvent(event);
        break;
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
