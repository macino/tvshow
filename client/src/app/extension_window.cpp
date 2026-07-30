#include "tvshow/app/extension_window.hpp"

#include "tvshow/app/extension_ui_protocol.hpp"

#define Uses_TEvent
#define Uses_TFrame
#define Uses_TKeys
#define Uses_TListViewer
#define Uses_TScrollBar
#include <tvision/tv.h>

#include <array>
#include <cstring>
#include <string>
#include <utility>

namespace tvshow::app {

namespace {

constexpr int kInputHeight = 1;
constexpr unsigned short kButtonCmdBase = 1000;
constexpr int kButtonHeight = 2;  // TButton's draw loop runs y=0..size.y-2; size.y=1 is invisible

}  // namespace

// Read-only scrollback list backed directly by ExtensionWindow::lines_
// (mirrors BrowserView's OptionListViewer pattern: TListViewer + a pointer
// to an externally-owned vector, no data duplication).
class ScrollbackViewer : public TListViewer {
public:
    ScrollbackViewer(const TRect& bounds, TScrollBar* sb, const std::vector<std::string>* lines)
        : TListViewer(bounds, 1, nullptr, sb), lines_(lines) {}

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void getText(char* dest, short item, short maxLen) override {
        const auto idx = static_cast<size_t>(item);
        if (idx < lines_->size()) {
            std::strncpy(dest, (*lines_)[idx].c_str(), static_cast<size_t>(maxLen));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        dest[maxLen] = '\0';
    }

    void sync() {
        setRange(static_cast<short>(lines_->size()));
        if (range > 0) { focusItem(static_cast<short>(range - 1)); }  // auto-scroll to bottom
        drawView();
    }

private:
    const std::vector<std::string>* lines_;
};

ExtensionWindow::ExtensionWindow(const TRect& bounds, const char* win_title,
                                 std::vector<std::string> argv)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, win_title, wnNoNumber) {
    process_ = std::make_unique<ExtensionProcess>(std::move(argv));

    const TRect inner = getExtent().grow(-1, -1);
    const TRect input_rect{inner.a.x, inner.b.y - kInputHeight, inner.b.x, inner.b.y};
    input_ = new TInputLine(input_rect, 255);
    insert(input_);

    const TRect sb_rect{inner.b.x - 1, inner.a.y, inner.b.x, inner.b.y - kInputHeight};
    auto* sb = new TScrollBar(sb_rect);
    insert(sb);

    const TRect view_rect{inner.a.x, inner.a.y, inner.b.x - 1, inner.b.y - kInputHeight};
    auto* viewer = new ScrollbackViewer(view_rect, sb, &lines_);
    insert(viewer);
    viewer_ = viewer;

    if (!process_->alive()) {
        append_line("[failed to start extension -- check the configured command]");
    }

    input_->select();
}

void ExtensionWindow::append_line(std::string line) {
    lines_.push_back(std::move(line));
    if (viewer_ != nullptr) { viewer_->sync(); }
}

void ExtensionWindow::clear_ui_widgets() {
    for (auto& [id, btn] : buttons_) { TObject::destroy(btn); }
    buttons_.clear();
    for (auto& [id, label] : texts_) { TObject::destroy(label); }
    texts_.clear();
}

void ExtensionWindow::handle_protocol_line(std::string_view line) {
    const UiCommand cmd = parse_ui_command(line);

    if (std::holds_alternative<UiClear>(cmd)) {
        clear_ui_widgets();
        return;
    }
    if (std::holds_alternative<UiTitle>(cmd)) {
        title_storage_ = std::get<UiTitle>(cmd).value;
        // TWindow::~TWindow() unconditionally `delete[]`s title (newStr()-
        // allocated by TWindow's own constructor) -- pointing it straight
        // at title_storage_.c_str() is an invalid-free waiting to happen
        // at window close (same bug class fixed in PuzzleWindow::relabel(),
        // see its comment). Match the constructor's allocation instead.
        delete[] const_cast<char*>(title);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
        title = newStr(title_storage_);
        frame->drawView();
        return;
    }
    if (const auto* b = std::get_if<UiButton>(&cmd)) {
        const TRect r{b->x, b->y, b->x + b->w, b->y + kButtonHeight};
        auto it = buttons_.find(b->id);
        if (it != buttons_.end()) {
            it->second->changeBounds(r);
        } else {
            auto* btn = new TButton(
                r, b->label.c_str(),
                static_cast<unsigned short>(kButtonCmdBase + static_cast<unsigned>(b->id)),
                bfNormal);
            insert(btn);
            buttons_[b->id] = btn;
        }
        return;
    }
    if (const auto* t = std::get_if<UiText>(&cmd)) {
        const TRect r{t->x, t->y, t->x + t->w, t->y + t->h};
        auto it = texts_.find(t->id);
        if (it != texts_.end()) {
            it->second->changeBounds(r);
            it->second->set_text(t->value);
        } else {
            auto* label = new LabelView(r);
            insert(label);
            label->set_text(t->value);
            texts_[t->id] = label;
        }
        return;
    }
    // UiInit / UiIgnored: no-op here (UI_INIT is consumed by poll() before reaching this point).
}

void ExtensionWindow::poll() {
    if (process_ == nullptr) {
        return;
    }
    const std::string chunk = process_->read_available();
    if (!chunk.empty()) {
        pending_line_ += chunk;
        size_t start = 0;
        while (true) {
            const auto nl = pending_line_.find('\n', start);
            if (nl == std::string::npos) { break; }
            std::string line = pending_line_.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            start = nl + 1;

            if (!first_line_seen_) {
                first_line_seen_ = true;
                if (line == "UI_INIT") {
                    ui_mode_ = true;
                    // Tear down the plain-scrollback chrome -- this extension speaks the
                    // structured protocol instead, no input line or log to show.
                    if (input_ != nullptr) { TObject::destroy(input_); input_ = nullptr; }
                    if (viewer_ != nullptr) { TObject::destroy(viewer_); viewer_ = nullptr; }
                    continue;
                }
            }
            if (ui_mode_) {
                handle_protocol_line(line);
            } else {
                append_line(line);
            }
        }
        pending_line_.erase(0, start);
    }
    if (!process_->alive() && !exited_notice_shown_) {
        exited_notice_shown_ = true;
        if (!ui_mode_) { append_line("[extension exited]"); }
    }
}

void ExtensionWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);

    if (ui_mode_) {
        if (event.what == evCommand &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
            event.message.command >= kButtonCmdBase &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
            event.message.command < kButtonCmdBase + 1000) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
            const int id = static_cast<int>(event.message.command - kButtonCmdBase);  // NOLINT(cppcoreguidelines-pro-type-union-access)
            if (process_ != nullptr) { process_->write_line(format_click_event(id)); }
            clearEvent(event);
        }
        return;
    }

    if (event.what == evKeyDown &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.keyDown.keyCode == kbEnter &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        input_ != nullptr) {
        std::array<char, 256> buf{};
        input_->getData(buf.data());
        const std::string text(buf.data());
        if (!text.empty() && process_ != nullptr) {
            append_line("> " + text);
            process_->write_line(text);
        }
        buf.fill('\0');
        input_->setData(buf.data());
        clearEvent(event);
    }
}

void ExtensionWindow::reposition(const TRect& inner) {
    const TRect input_rect{inner.a.x, inner.b.y - kInputHeight, inner.b.x, inner.b.y};
    if (input_ != nullptr) { input_->changeBounds(input_rect); }
    const TRect view_rect{inner.a.x, inner.a.y, inner.b.x - 1, inner.b.y - kInputHeight};
    if (viewer_ != nullptr) { viewer_->changeBounds(view_rect); }
    // Structured UI-mode widgets keep the absolute x/y/w/h the child specified --
    // no reflow on resize in v1 (documented limitation, adr-extension-ui-protocol).
}

void ExtensionWindow::changeBounds(const TRect& bounds) {
    TWindow::changeBounds(bounds);
    reposition(getExtent().grow(-1, -1));
}

}  // namespace tvshow::app
