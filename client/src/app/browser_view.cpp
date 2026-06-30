#include "tvshow/app/browser_view.hpp"

#include "tvshow/app/builtin_themes.hpp"
#include "tvshow/css/parser.hpp"

#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TEventQueue
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TListViewer
#define Uses_TFrame
#define Uses_TScrollBar
#define Uses_TWindow
#define Uses_TStaticText
#include "tvshow/app/bookmarks.hpp"
#include "tvshow/app/page.hpp"
#include "tvshow/layout/box.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/form.hpp"
#include "tvshow/layout/form_data.hpp"
#include "tvshow/layout/form_focus.hpp"
#include "tvshow/layout/links.hpp"
#include "tvshow/paint/paint.hpp"
#include "tvshow/render/chargrid.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/types.hpp"
#include "tvshow/util/url.hpp"

#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
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

// Modal dialog with a single input line; closes on Enter or Esc.
class FindDialog : public TDialog {
public:
    FindDialog(const TRect& r, const char* dlg_title)
        : TWindowInit(&TWindow::initFrame), TDialog(r, dlg_title) {}

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    void handleEvent(TEvent& event) override {
        if (event.what == evKeyDown &&
            event.keyDown.keyCode == kbEnter) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
            endModal(cmOK);
            clearEvent(event);
            return;
        }
        TDialog::handleEvent(event);
    }
};

// Decode a UTF-8 string to a vector of Unicode codepoints.
std::vector<char32_t> utf8_to_u32(std::string_view s) {
    std::vector<char32_t> out;
    const auto* p = reinterpret_cast<const unsigned char*>(s.data());
    const auto* end = p + s.size();
    while (p < end) {
        char32_t cp = 0;
        const unsigned char c = *p;
        if (c < 0x80U) {
            cp = c;
            p += 1;
        } else if (c < 0xE0U && end - p >= 2) {
            cp = (static_cast<char32_t>(c & 0x1FU) << 6U) | (p[1] & 0x3FU);
            p += 2;
        } else if (c < 0xF0U && end - p >= 3) {
            cp = (static_cast<char32_t>(c & 0x0FU) << 12U) |
                 (static_cast<char32_t>(p[1] & 0x3FU) << 6U) | (p[2] & 0x3FU);
            p += 3;
        } else if (end - p >= 4) {
            cp = (static_cast<char32_t>(c & 0x07U) << 18U) |
                 (static_cast<char32_t>(p[1] & 0x3FU) << 12U) |
                 (static_cast<char32_t>(p[2] & 0x3FU) << 6U) | (p[3] & 0x3FU);
            p += 4;
        } else {
            ++p;
            continue;
        }
        out.push_back(cp);
    }
    return out;
}

// ── BookmarkPickerDialog ──────────────────────────────────────────────────────
// Shows the bookmark list. Keys:
//   Enter / dbl-click  — navigate (endModal cmOK)
//   'a'                — add current URL (endModal cmYes)
//   'd' / kbDel        — delete selected (endModal cmNo)
//   Esc                — cancel

class BookmarkListViewer : public TListViewer {
public:
    BookmarkListViewer(TRect r, TScrollBar* sb, std::vector<std::string>* labels)
        : TListViewer(r, 1, nullptr, sb), labels_(labels) {
        setRange(static_cast<int>(labels->size()));
    }

    void getText(char* dest, short item, short maxLen) override {
        if (item < 0 || item >= static_cast<short>(labels_->size())) { dest[0] = '\0'; return; }
        const auto& s = (*labels_)[static_cast<size_t>(item)];
        std::strncpy(dest, s.c_str(), static_cast<size_t>(maxLen));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dest[maxLen] = '\0';
    }

    // Refresh after add/delete.
    void refresh() {
        setRange(static_cast<int>(labels_->size()));
        if (focused >= range) { focusItem(std::max(0, range - 1)); }
        drawView();
    }

private:
    std::vector<std::string>* labels_;
};

// OSC 52 clipboard: write text to the terminal's clipboard selection 'c'.
// Sequence: ESC ] 52 ; c ; <base64(text)> BEL
void osc52_copy(std::string_view text) {
    static constexpr std::string_view k_table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    b64.reserve(((text.size() + 2) / 3) * 4);
    const auto* data = reinterpret_cast<const unsigned char*>(text.data());
    for (std::size_t i = 0; i < text.size(); i += 3) {
        const unsigned a = data[i];
        const unsigned b = (i + 1 < text.size()) ? data[i + 1] : 0U;
        const unsigned c = (i + 2 < text.size()) ? data[i + 2] : 0U;
        b64 += k_table[(a >> 2U) & 0x3FU];
        b64 += k_table[((a << 4U) | (b >> 4U)) & 0x3FU];
        b64 += (i + 1 < text.size()) ? k_table[((b << 2U) | (c >> 6U)) & 0x3FU] : '=';
        b64 += (i + 2 < text.size()) ? k_table[c & 0x3FU] : '=';
    }
    // Write the OSC 52 sequence directly to stdout (bypasses TurboVision drawing).
    std::printf("\033]52;c;%s\007", b64.c_str());
    std::fflush(stdout);
}

}  // namespace

namespace tvshow::app {

BrowserView::BrowserView(const TRect& bounds, Page page, SharedBrowsingState* shared)
    : TView(bounds), page_(std::move(page)), shared_(shared) {
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evKeyDown | evMouseDown | evMouseWheel | evMouseMove;
    links_ = layout::collect_links(page_.box);
    form_controls_ = layout::collect_form_controls(page_.box);
    history_.push_back(page_.url);
    record_visit(page_.url);
}

BrowserView::~BrowserView() {
    load_cancelled_.store(true, std::memory_order_release);
    if (loader_thread_.joinable()) {
        loader_thread_.join();
    }
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

// Translate a point from collapsed (visual) row space to original (layout) row space.
// kept_rows[collapsed_row] = original_row.
static Point uncollapse_pt(Point pt, const std::vector<int>& kept_rows) {
    const int r = pt.row;
    if (!kept_rows.empty() && r >= 0 && r < static_cast<int>(kept_rows.size())) {
        return {pt.col, kept_rows[static_cast<size_t>(r)]};
    }
    return pt;
}

// Map a span's rows from original (layout) space to collapsed (visual) space.
// Rows that were collapsed away are dropped; rows that survive get their
// row coordinate remapped.  Column coordinates are unchanged.
static std::vector<layout::CellRect> map_spans(const std::vector<layout::CellRect>& spans,
                                               const std::vector<int>& kept_rows) {
    // Build inverse: original_row → collapsed_row (-1 if removed).
    int max_orig = 0;
    for (int k : kept_rows) max_orig = std::max(max_orig, k);
    std::vector<int> orig_to_col(static_cast<size_t>(max_orig + 1), -1);
    for (int i = 0; i < static_cast<int>(kept_rows.size()); ++i) {
        orig_to_col[static_cast<size_t>(kept_rows[static_cast<size_t>(i)])] = i;
    }

    std::vector<layout::CellRect> out;
    out.reserve(spans.size());
    for (const auto& s : spans) {
        const int r0 = s.origin.row;
        const int r1 = r0 + s.size.rows - 1;
        if (r0 < 0 || r0 > max_orig || r1 > max_orig) continue;
        const int c0 = orig_to_col[static_cast<size_t>(r0)];
        if (c0 < 0) continue;
        // Find the last kept row within the span.
        int c1 = c0;
        for (int r = r1; r >= r0; --r) {
            if (r <= max_orig && orig_to_col[static_cast<size_t>(r)] >= 0) {
                c1 = orig_to_col[static_cast<size_t>(r)];
                break;
            }
        }
        out.push_back({{s.origin.col, c0}, {s.size.cols, c1 - c0 + 1}});
    }
    return out;
}

void BrowserView::ensure_display_grid() const {
    auto* self = const_cast<BrowserView*>(this);

    if (base_dirty_) {
        const render::RenderOpts opts{page_.url, &visited_set()};
        auto [grid, kept] = render::collapse_blank_rows(
            render::render(page_.box, form_values_, opts));
        self->base_grid_ = std::move(grid);
        self->kept_rows_ = std::move(kept);
        self->base_dirty_ = false;
        self->overlay_dirty_ = true;
    }

    const bool focus_changed = (focused_ != cached_focused_);
    const bool hits_changed  = (search_hits_ != cached_hits_ || search_hit_idx_ != cached_hit_idx_);

    if (overlay_dirty_ || focus_changed || hits_changed) {
        self->display_grid_ = *base_grid_;  // copy base, then apply overlays
        render::CharGrid& g = *self->display_grid_;

        if (is_link_focused()) {
            render::apply_focus(g, map_spans(links_[static_cast<size_t>(focused_)].spans, kept_rows_));
        } else if (const layout::FormFocus* fc = focused_fc()) {
            render::apply_focus(g, map_spans({fc->span}, kept_rows_));
        }

        if (!search_hits_.empty() && search_len_ > 0) {
            for (int i = 0; i < static_cast<int>(search_hits_.size()); ++i) {
                const Point& p = search_hits_[static_cast<size_t>(i)];
                const bool is_current = (i == search_hit_idx_);
                for (int dc = 0; dc < search_len_; ++dc) {
                    const int col = p.col + dc;
                    if (col < 0 || col >= g.cols() || p.row < 0 || p.row >= g.rows()) {
                        continue;
                    }
                    const render::Cell cell = g.at({col, p.row});
                    render::ColorAttr attr = cell.attr;
                    if (is_current) {
                        attr.bg = 0xFFCC00U;
                        attr.fg = 0x000000U;
                    } else {
                        attr.bg = 0x555500U;
                        attr.fg = 0xFFFFFFU;
                    }
                    g.put({col, p.row}, cell.cp, attr);
                }
            }
        }

        if (debug_overlay_) {
            render::apply_debug_overlay(g, page_.box);
        }

        self->overlay_dirty_ = false;
        self->cached_focused_ = focused_;
        self->cached_hits_ = search_hits_;
        self->cached_hit_idx_ = search_hit_idx_;
    }
}

void BrowserView::draw() {
    if (loading_.load(std::memory_order_acquire)) {
        static constexpr std::array<const char*, 8> k_frames = {
            "\xE2\xA3\xBE", "\xE2\xA3\xBD", "\xE2\xA3\xBB", "\xE2\xA2\xBF",
            "\xE2\xA1\xBF", "\xE2\xA3\x9F", "\xE2\xA3\xAF", "\xE2\xA3\xB7",
        };
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - load_start_)
                            .count();
        const char* spin = k_frames[static_cast<size_t>((ms / 120) % 8)];
        const std::string label = std::string(spin) + " Loading...";

        TDrawBuffer buf;
        buf.moveChar(0, ' ', TColorAttr(0x07), static_cast<short>(size.x));
        buf.moveStr(0, label.c_str(), TColorAttr(0x07));
        writeLine(0, 0, static_cast<short>(size.x), 1, buf);

        TDrawBuffer blank;
        blank.moveChar(0, ' ', TColorAttr(0x07), static_cast<short>(size.x));
        for (short row = 1; row < size.y; ++row) {
            writeLine(0, row, static_cast<short>(size.x), 1, blank);
        }
        return;
    }

    ensure_display_grid();
    TDrawBuffer buf;
    const render::CharGrid& grid = *display_grid_;
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

    // Update window title with scroll position indicator.
    if (owner != nullptr) {
        const int total = page_.box.border_box.size.rows;
        if (total > size.y) {
            const int pct = (total > 0) ? (scroll_row_ * 100 / total) : 0;
            char title_buf[512];
            std::snprintf(title_buf, sizeof(title_buf), "%s [%d%%]",
                          page_.url.c_str(), pct);
            auto* win = dynamic_cast<TWindow*>(owner);
            if (win != nullptr) {
                // Frees old title and allocates new one.
                delete[] win->title;  // NOLINT
                win->title = newStr(title_buf);
                win->frame->drawView();
            }
        }
    }
}

void BrowserView::scroll_to(int row) {
    scroll_row_ = std::clamp(row, 0, scroll_limit());
    sync_vscroll();
    drawView();
}

void BrowserView::relayout() {
    base_dirty_ = true;
    overlay_dirty_ = true;
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
    // Cancel any in-flight load and wait for it to finish.
    load_cancelled_.store(true, std::memory_order_release);
    if (loader_thread_.joinable()) {
        loader_thread_.join();
    }

    // Strip fragment (client-side only).
    const auto hash_pos = url.rfind('#');
    const std::string base_url = (hash_pos != std::string::npos) ? url.substr(0, hash_pos) : url;
    const std::string fragment =
        (hash_pos != std::string::npos) ? url.substr(hash_pos + 1) : std::string{};

    {
        std::lock_guard<std::mutex> lk(pending_mutex_);
        pending_load_.reset();
    }
    page_ready_.store(false, std::memory_order_relaxed);
    load_cancelled_.store(false, std::memory_order_release);
    loading_.store(true, std::memory_order_release);
    load_start_ = std::chrono::steady_clock::now();
    drawView();

    const std::string fetch_url = base_url.empty() ? url : base_url;
    const Size vp{size.x, size.y};

    loader_thread_ = std::thread([this, fetch_url, url, fragment, push_history, vp]() {
        // Inner thread does the blocking HTTP fetch.
        std::atomic<bool> inner_done{false};
        std::optional<Page> loaded;
        std::thread inner([&]() {
            try {
                net::CookieJar* jar = (shared_ != nullptr) ? &shared_->cookie_jar : nullptr;
                loaded = load_page(fetch_url, vp, jar);
            } catch (...) {
                // loaded stays nullopt; apply_loaded_page handles it
            }
            inner_done.store(true, std::memory_order_release);
        });

        // Outer loop wakes the event loop for animation while inner is running.
        using namespace std::chrono_literals;
        while (!inner_done.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(120ms);
            if (!load_cancelled_.load(std::memory_order_acquire)) {
                TEventQueue::wakeUp();  // thread-safe; drives spinner via Application::idle()
            }
        }
        inner.join();

        {
            std::lock_guard<std::mutex> lk(pending_mutex_);
            pending_load_.emplace(PendingLoad{std::move(loaded), url, fragment, push_history});
        }
        page_ready_.store(true, std::memory_order_release);

        if (!load_cancelled_.load(std::memory_order_acquire)) {
            TEventQueue::wakeUp();  // wake once more to apply the loaded page in idle()
        }
    });
}

void BrowserView::apply_loaded_page() {
    PendingLoad result;
    {
        std::lock_guard<std::mutex> lk(pending_mutex_);
        if (!pending_load_) {
            return;
        }
        result = std::move(*pending_load_);
        pending_load_.reset();
    }
    page_ready_.store(false, std::memory_order_relaxed);
    loading_.store(false, std::memory_order_release);

    if (!result.page) {
        drawView();
        return;
    }

    page_ = std::move(*result.page);
    record_visit(page_.url);
    form_values_ = {};
    focused_ = -1;
    scroll_row_ = 0;
    // If a forced style is active, re-resolve immediately so the new page
    // renders with the override rather than its own author CSS.
    if (shared_ != nullptr && shared_->forced_style != ForcedStyle::Auto) {
        forced_sheets_css_ = nullptr;  // invalidate cache for new page URL context
        forced_sheets_.clear();
        if (auto new_tree = style::resolve(page_.doc, effective_sheets())) {
            page_.tree = std::make_unique<style::StyledNode>(std::move(*new_tree));
            page_.sheets = {};
        }
    }
    relayout();

    if (!result.fragment.empty()) {
        const int anchor = layout::find_anchor_row(page_.box, result.fragment);
        if (anchor >= 0) {
            scroll_row_ = std::clamp(anchor, 0, scroll_limit());
            sync_vscroll();
        }
    }

    if (result.push_history) {
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_pos_) + 1,
                       history_.end());
        history_.push_back(result.url);
        history_pos_ = history_.size() - 1;
    }
    drawView();
}

void BrowserView::tick_if_loading() {
    if (page_ready_.load(std::memory_order_acquire)) {
        apply_loaded_page();
    } else if (loading_.load(std::memory_order_acquire)) {
        drawView();
    }
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
        base_dirty_ = true;
        drawView();
    } else if (char_code == 0x20U) {
        // Space toggles checkbox / selects radio.
        if (fc->kind == layout::FormControlKind::Checkbox) {
            auto it = form_values_.checked.find(fc->node);
            const bool was = (it != form_values_.checked.end())
                                 ? it->second
                                 : (fc->node->attr("checked").data() != nullptr);
            form_values_.checked[fc->node] = !was;
            base_dirty_ = true;
            drawView();
        } else if (fc->kind == layout::FormControlKind::Radio) {
            form_values_.checked[fc->node] = true;
            base_dirty_ = true;
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
    base_dirty_ = true;
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
        net::CookieJar* jar = (shared_ != nullptr) ? &shared_->cookie_jar : nullptr;
        auto page = post_page(action_url, encoded, {size.x, size.y}, jar);
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
    pt = uncollapse_pt(pt, kept_rows_);
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

    // Mouse move: update hover state.
    if (event.what == evMouseMove) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const TPoint local = makeLocal(event.mouse.where);
        update_hover({local.x, local.y + scroll_row_});
        clearEvent(event);
        return;
    }

    // Mouse wheel scroll.
    if (event.what == evMouseWheel) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        const auto wheel = event.mouse.wheel;
        if ((wheel & mwDown) != 0) {
            scroll_to(scroll_row_ + 3);
        } else if ((wheel & mwUp) != 0) {
            scroll_to(scroll_row_ - 3);
        }
        clearEvent(event);
        return;
    }

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
    case kbCtrlF:
        show_find_dialog();
        clearEvent(event);
        break;
    case 0x0002:  // Ctrl-B
        show_bookmarks_dialog();
        clearEvent(event);
        break;
    case 0x0003:  // Ctrl-C — copy focused link URL via OSC 52
        if (is_link_focused()) {
            const std::string abs_url = util::resolve_url(
                page_.url, links_[static_cast<size_t>(focused_)].href);
            osc52_copy(abs_url);
            clearEvent(event);
        }
        break;
    case 0x0004:  // Ctrl-D
        debug_overlay_ = !debug_overlay_;
        overlay_dirty_ = true;
        drawView();
        clearEvent(event);
        break;
    case kbEsc:
        if (!search_term_.empty()) {
            search_term_.clear();
            search_hits_.clear();
            search_hit_idx_ = -1;
            search_len_ = 0;
            drawView();
            clearEvent(event);
        }
        break;
    default: {
        const unsigned char_code = keyCode & 0xFFU;
        const bool is_backspace = char_code == 0x08U;
        const bool is_printable = char_code >= 0x20U && char_code < 0x7FU;
        if (focused_fc() != nullptr && (is_backspace || is_printable)) {
            handle_form_input(keyCode);
            clearEvent(event);
        } else if (!search_hits_.empty() && focused_fc() == nullptr) {
            if (char_code == 'n') {
                navigate_to_hit(1);
                drawView();
                clearEvent(event);
            } else if (char_code == 'N') {
                navigate_to_hit(-1);
                drawView();
                clearEvent(event);
            }
        }
        break;
    }
    }
}

void BrowserView::show_find_dialog() {
    if (owner == nullptr || owner->owner == nullptr) {
        return;
    }
    TGroup* const desk = owner->owner;
    constexpr int kMaxTerm = 127;
    constexpr int kDlgW = 44;
    constexpr int kDlgH = 3;
    const TRect desk_r = desk->getBounds();
    const TRect dlg_r{(desk_r.b.x - kDlgW) / 2, (desk_r.b.y - kDlgH) / 2,
                      (desk_r.b.x + kDlgW) / 2, (desk_r.b.y + kDlgH) / 2};
    auto* dlg = new FindDialog(dlg_r, "Find");
    const TRect bar_r{1, 1, kDlgW - 2, 2};
    auto* bar = new TInputLine(bar_r, kMaxTerm);
    if (!search_term_.empty()) {
        std::array<char, kMaxTerm + 1> ibuf{};
        std::strncpy(ibuf.data(), search_term_.c_str(), kMaxTerm);
        bar->setData(ibuf.data());
    }
    dlg->insert(bar);
    bar->select();
    const unsigned short res = desk->execView(dlg);
    std::array<char, kMaxTerm + 1> rbuf{};
    bar->getData(rbuf.data());
    TObject::destroy(dlg);
    if (res != cmOK) {
        return;
    }
    find_matches_in_page(rbuf.data());
    navigate_to_hit(0);
    drawView();
}

void BrowserView::find_matches_in_page(std::string_view term) {
    search_term_ = std::string(term);
    search_hits_.clear();
    search_hit_idx_ = -1;
    search_len_ = 0;
    if (term.empty()) {
        return;
    }
    const std::vector<char32_t> needle = utf8_to_u32(term);
    if (needle.empty()) {
        return;
    }
    search_len_ = static_cast<int>(needle.size());
    // Ensure base_grid_ is up to date (rebuilds if dirty, no-op otherwise).
    if (base_dirty_) {
        const render::RenderOpts opts{page_.url, &visited_set()};
        auto [g, kept] = render::collapse_blank_rows(
            render::render(page_.box, form_values_, opts));
        base_grid_ = std::move(g);
        kept_rows_ = std::move(kept);
        base_dirty_ = false;
        overlay_dirty_ = true;
    }
    const render::CharGrid& grid = *base_grid_;
    const int len = static_cast<int>(needle.size());
    for (int row = 0; row < grid.rows(); ++row) {
        for (int col = 0; col + len <= grid.cols(); ++col) {
            bool match = true;
            for (int k = 0; k < len; ++k) {
                const char32_t cell_cp = grid.at({col + k, row}).cp;
                const char32_t n_cp = needle[static_cast<size_t>(k)];
                const bool eq = (cell_cp < 128U && n_cp < 128U)
                    ? (static_cast<char32_t>(std::tolower(static_cast<int>(cell_cp))) ==
                       static_cast<char32_t>(std::tolower(static_cast<int>(n_cp))))
                    : (cell_cp == n_cp);
                if (!eq) {
                    match = false;
                    break;
                }
            }
            if (match) {
                search_hits_.push_back({col, row});
            }
        }
    }
    if (!search_hits_.empty()) {
        search_hit_idx_ = 0;
    }
}

void BrowserView::navigate_to_hit(int dir) {
    if (search_hits_.empty()) {
        return;
    }
    const int n = static_cast<int>(search_hits_.size());
    if (search_hit_idx_ < 0) {
        search_hit_idx_ = 0;
    } else {
        search_hit_idx_ = (search_hit_idx_ + dir + n) % n;
    }
    sync_vscroll();
    const int target_row = search_hits_[static_cast<size_t>(search_hit_idx_)].row;
    scroll_to(std::max(0, target_row - size.y / 2));
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

void BrowserView::show_bookmarks_dialog() {
    if (owner == nullptr || owner->owner == nullptr) { return; }
    TGroup* const desk = owner->owner;

    BookmarkStore& store =
        (shared_ != nullptr) ? shared_->bookmarks : local_bookmarks_;

    // Build display labels: "title  url" or just "url".
    std::vector<std::string> labels;
    auto rebuild_labels = [&]() {
        labels.clear();
        for (const auto& bm : store.bookmarks()) {
            labels.push_back(bm.title.empty() ? bm.url : bm.title + "  " + bm.url);
        }
    };
    rebuild_labels();

    constexpr int kDlgW = 70;
    constexpr int kDlgH = 18;
    const TRect desk_r = desk->getBounds();
    const TRect dlg_r{(desk_r.b.x - kDlgW) / 2, (desk_r.b.y - kDlgH) / 2,
                      (desk_r.b.x + kDlgW) / 2, (desk_r.b.y + kDlgH) / 2};

    // Local subclass — catches 'a'/'d'/Enter/Esc at the dialog level.
    struct BookmarkDialog : TDialog {
        BookmarkListViewer* viewer_;
        std::vector<std::string>* labels_;
        BookmarkStore* store_;
        std::string current_url_;

        BookmarkDialog(TRect r, const char* t, BookmarkListViewer* v,
                       std::vector<std::string>* l, BookmarkStore* s, std::string cu)
            : TWindowInit(&TWindow::initFrame), TDialog(r, t),
              viewer_(v), labels_(l), store_(s), current_url_(std::move(cu)) {}

        void sync_labels() {
            labels_->clear();
            for (const auto& bm : store_->bookmarks()) {
                labels_->push_back(bm.title.empty() ? bm.url : bm.title + "  " + bm.url);
            }
            viewer_->refresh();
        }

        void handleEvent(TEvent& event) override {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            if (event.what != evKeyDown) { TDialog::handleEvent(event); return; }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            const unsigned kc = event.keyDown.keyCode;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            const unsigned ch = event.keyDown.charScan.charCode;

            if (ch == 'a' || ch == 'A') {
                if (!store_->contains(current_url_)) {
                    store_->add(current_url_);
                    (void)save_bookmarks(*store_);
                    sync_labels();
                }
                clearEvent(event);
                return;
            }
            if ((ch == 'd' || ch == 'D' || kc == kbDel) && viewer_->range > 0) {
                const int idx = viewer_->focused;
                const auto& bms = store_->bookmarks();
                if (idx >= 0 && idx < static_cast<int>(bms.size())) {
                    store_->remove(bms[static_cast<size_t>(idx)].url);
                    (void)save_bookmarks(*store_);
                    sync_labels();
                }
                clearEvent(event);
                return;
            }
            if (kc == kbEnter && viewer_->range > 0) {
                endModal(cmOK);
                clearEvent(event);
                return;
            }
            if (kc == kbEsc) {
                endModal(cmCancel);
                clearEvent(event);
                return;
            }
            TDialog::handleEvent(event);
        }
    };

    auto* sb = new TScrollBar({kDlgW - 2, 1, kDlgW - 1, kDlgH - 2});
    auto* viewer = new BookmarkListViewer({1, 1, kDlgW - 2, kDlgH - 2}, sb, &labels);
    auto* hint = new TStaticText({1, kDlgH - 2, kDlgW - 1, kDlgH - 1},
                                 "Enter:open  A:add  D:delete  Esc:cancel");
    auto* bdlg = new BookmarkDialog(dlg_r, "Bookmarks", viewer, &labels, &store, page_.url);
    bdlg->insert(sb);
    bdlg->insert(viewer);
    bdlg->insert(hint);

    const unsigned short res = desk->execView(bdlg);
    const short sel = viewer->focused;
    const int n = static_cast<int>(store.bookmarks().size());
    TObject::destroy(bdlg);

    if (res == cmOK && sel >= 0 && sel < n) {
        navigate_to(store.bookmarks()[static_cast<size_t>(sel)].url);
    }
    drawView();
}

void BrowserView::update_hover(Point content_pt) {
    content_pt = uncollapse_pt(content_pt, kept_rows_);
    const layout::Box* box = layout::hit_test(page_.box, content_pt);
    const dom::Node* node = (box != nullptr && box->node != nullptr) ? box->node->node : nullptr;
    if (node == hovered_node_) {
        return;
    }
    hovered_node_ = node;
    hovered_set_.clear();
    if (node != nullptr) {
        // Build ancestor chain: walk up through the box tree is hard, so
        // just put the hovered node itself. CSS :hover on ancestors would
        // need a parent-pointer walk, but for v1 this covers the common case.
        hovered_set_.insert(node);
    }
    restyle_for_hover();
}

// Returns the stylesheet list to use for style resolution: the forced theme
// overrides the page's own author sheets when ForcedStyle != Auto.
const std::vector<css::Stylesheet>& BrowserView::effective_sheets() const {
    if (shared_ == nullptr || shared_->forced_style == ForcedStyle::Auto) {
        return page_.sheets;
    }
    const char* css_src = nullptr;
    switch (shared_->forced_style) {
        case ForcedStyle::Tvision: css_src = k_css_tvision; break;
        case ForcedStyle::Light:   css_src = k_css_light;   break;
        case ForcedStyle::Dark:    css_src = k_css_dark;    break;
        default: return page_.sheets;
    }
    if (forced_sheets_css_ != css_src) {
        forced_sheets_css_ = css_src;
        forced_sheets_.clear();
        if (auto sheet = css::parse(css_src)) {
            forced_sheets_.push_back(std::move(*sheet));
        }
    }
    return forced_sheets_;
}

void BrowserView::restyle_for_hover() {
    const style::ResolveOpts opts{hovered_set_.empty() ? nullptr : &hovered_set_};
    auto new_tree = style::resolve(page_.doc, effective_sheets(), opts);
    if (!new_tree) {
        return;
    }
    page_.tree = std::make_unique<style::StyledNode>(std::move(*new_tree));
    relayout();
    drawView();
}

void BrowserView::apply_forced_style() {
    forced_sheets_css_ = nullptr;  // invalidate cache so effective_sheets() re-parses
    forced_sheets_.clear();
    restyle_for_hover();           // re-resolves + relayout + drawView
}

}  // namespace tvshow::app
