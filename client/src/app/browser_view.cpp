#include "tvshow/app/browser_view.hpp"

#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TListViewer
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
#include "tvshow/types.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Modal list for <select> option picking.
class OptionListViewer : public TListViewer {
public:
    OptionListViewer(const TRect& bounds, const std::vector<std::string>* labels)
        : TListViewer(bounds, 1, nullptr, nullptr), labels_(labels) {
        setRange(static_cast<short>(labels_->size()));
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void getText(char* dest, short item, short maxLen) override {
        const auto idx = static_cast<size_t>(item);
        if (idx < labels_->size()) {
            std::strncpy(dest, (*labels_)[idx].c_str(), static_cast<size_t>(maxLen));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dest[maxLen] = '\0';
    }

    void handleEvent(TEvent& event) override {
        const bool is_key =
            event.what == evKeyDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool is_down =
            event.what == evMouseDown;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool dbl_click = is_down && ((event.mouse.eventFlags &  // NOLINT(cppcoreguidelines-pro-type-union-access)
                                            meDoubleClick) != 0);
        const bool enter_key =
            is_key &&
            (event.keyDown.keyCode == kbEnter);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const bool esc_key =
            is_key &&
            (event.keyDown.keyCode == kbEsc);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        if (dbl_click || enter_key) {
            endModal(cmOK);
            clearEvent(event);
        } else if (esc_key) {
            endModal(cmCancel);
            clearEvent(event);
        } else {
            TListViewer::handleEvent(event);
        }
    }

private:
    const std::vector<std::string>* labels_;
};

}  // namespace

namespace tvshow::app {

BrowserView::BrowserView(const TRect& bounds, Page page, SharedBrowsingState* shared)
    : TView(bounds), page_(std::move(page)), shared_(shared) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evKeyDown | evMouseDown;
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    history_.push_back(page_.url);
    record_visit(page_.url);
}

void BrowserView::record_visit(const std::string& url) {
    if (shared_ != nullptr) {
        shared_->history.push_back(url);
        shared_->visited.insert(url);
    } else {
        local_visited_.insert(url);
    }
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
    const render::RenderOpts opts{page_.url, &visited_set()};
    render::CharGrid grid = render::render(page_.box, form_values_, opts);
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
    record_visit(page_.url);
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
        fc->kind == layout::FormControlKind::Password ||
        fc->kind == layout::FormControlKind::Textarea) {
        auto& val = form_values_.text[fc->node];
        if (char_code == 0x08U) {
            if (!val.empty()) {
                val.pop_back();
            }
        } else if (fc->kind == layout::FormControlKind::Textarea && char_code == 0x0DU) {
            val += '\n';
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

void BrowserView::cycle_select_option(int direction) {
    const layout::FormFocus* fc = focused_fc();
    if (fc == nullptr || fc->kind != layout::FormControlKind::Select) {
        return;
    }
    const dom::Node* node = fc->node;
    // Build ordered list of option value strings.
    std::vector<std::string_view> vals;
    for (const auto& cp : node->children) {
        if (cp->kind == dom::NodeKind::Element && cp->tag == "option") {
            vals.push_back(cp->attr("value"));
        }
    }
    if (vals.empty()) {
        return;
    }
    const auto it = form_values_.text.find(node);
    const std::string_view cur =
        (it != form_values_.text.end()) ? std::string_view(it->second) : node->attr("value");
    int idx = 0;
    for (int i = 0; i < static_cast<int>(vals.size()); ++i) {
        if (vals[static_cast<size_t>(i)] == cur) {
            idx = i;
            break;
        }
    }
    const int n = static_cast<int>(vals.size());
    form_values_.text[node] = std::string(vals[static_cast<size_t>((idx + direction + n) % n)]);
    drawView();
}

void BrowserView::show_select_popup(const layout::FormFocus& fc) {
    if (fc.node == nullptr || owner == nullptr || owner->owner == nullptr) {
        return;
    }
    // Build option label/value lists from the DOM.
    std::vector<std::string> labels;
    std::vector<std::string> values;
    for (const auto& cp : fc.node->children) {
        if (cp->kind != dom::NodeKind::Element || cp->tag != "option") {
            continue;
        }
        values.emplace_back(cp->attr("value"));
        std::string label;
        for (const auto& tc : cp->children) {
            if (tc->kind == dom::NodeKind::Text) {
                label = tc->text;
                break;
            }
        }
        labels.push_back(std::move(label));
    }
    if (labels.empty()) {
        return;
    }

    // Find current selection index.
    const auto it = form_values_.text.find(fc.node);
    const std::string_view cur_val =
        (it != form_values_.text.end()) ? std::string_view(it->second) : fc.node->attr("value");
    int sel_idx = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == cur_val) {
            sel_idx = static_cast<int>(i);
            break;
        }
    }

    // Build and run the dialog on the desktop (owner->owner).
    constexpr int kDlgW = 40;
    const int n = static_cast<int>(labels.size());
    const int h = std::min(n + 2, 14);
    TGroup* const desk = owner->owner;
    const TRect desk_r = desk->getBounds();
    const TRect dlg_r{(desk_r.b.x - kDlgW) / 2, (desk_r.b.y - h) / 2,
                      (desk_r.b.x + kDlgW) / 2, (desk_r.b.y + h) / 2};
    auto* dlg = new TDialog(dlg_r, "Select");
    const TRect list_r{1, 1, kDlgW - 2, h - 1};
    auto* viewer = new OptionListViewer(list_r, &labels);
    viewer->focusItem(static_cast<short>(sel_idx));
    dlg->insert(viewer);

    const unsigned short res = desk->execView(dlg);
    const short sel = viewer->focused;
    TObject::destroy(dlg);

    if (res == cmOK && sel >= 0 && sel < n) {
        form_values_.text[fc.node] = values[static_cast<size_t>(sel)];
        drawView();
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

bool BrowserView::handle_mouse_hit(Point pt, TEvent& event) {
    for (const auto& link : links_) {
        for (const auto& span : link.spans) {
            if (span.contains(pt)) {
                navigate(util::resolve_url(page_.url, link.href), true);
                clearEvent(event);
                return true;
            }
        }
    }
    for (size_t i = 0; i < form_controls_.size(); ++i) {
        if (!form_controls_[i].span.contains(pt)) {
            continue;
        }
        focused_ = static_cast<int>(links_.size() + i);
        const layout::FormFocus& fc = form_controls_[i];
        switch (fc.kind) {
        case layout::FormControlKind::Submit:
            clearEvent(event);
            submit_form();
            return true;
        case layout::FormControlKind::Checkbox: {
            const auto it = form_values_.checked.find(fc.node);
            const bool was = (it != form_values_.checked.end())
                                 ? it->second
                                 : (fc.node->attr("checked").data() != nullptr);
            form_values_.checked[fc.node] = !was;
            drawView();
            clearEvent(event);
            return true;
        }
        case layout::FormControlKind::Radio:
            form_values_.checked[fc.node] = true;
            drawView();
            clearEvent(event);
            return true;
        case layout::FormControlKind::Select:
            drawView();
            clearEvent(event);
            show_select_popup(fc);
            return true;
        default:
            drawView();
            clearEvent(event);
            return true;
        }
    }
    return false;
}

void BrowserView::handleEvent(TEvent& event) {
    TView::handleEvent(event);

    if (event.what == evMouseDown) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const TPoint local = makeLocal(event.mouse.where);
        if (handle_mouse_hit({local.x, local.y + scroll_row_}, event)) {
            return;
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
        if (const layout::FormFocus* fc = focused_fc();
            fc != nullptr && fc->kind == layout::FormControlKind::Select) {
            cycle_select_option(-1);
        } else {
            scroll_to(scroll_row_ - 1);
        }
        clearEvent(event);
        break;
    case kbDown:
        if (const layout::FormFocus* fc = focused_fc();
            fc != nullptr && fc->kind == layout::FormControlKind::Select) {
            cycle_select_option(1);
        } else {
            scroll_to(scroll_row_ + 1);
        }
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
        } else if (const layout::FormFocus* fc = focused_fc(); fc != nullptr) {
            if (fc->kind == layout::FormControlKind::Textarea) {
                handle_form_input(kbEnter);
            } else if (fc->kind == layout::FormControlKind::Select) {
                show_select_popup(*fc);
            } else {
                submit_form();
            }
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
