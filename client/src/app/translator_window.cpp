#include "tvshow/app/translator_window.hpp"

#define Uses_TDialog
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TLabel
#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace tvshow::app {

namespace {

constexpr unsigned short cmTranslateClick = 2000;
constexpr int kFieldMaxLen = 511;
constexpr int kLangFieldMaxLen = 8;
constexpr auto kRequestTimeout = std::chrono::seconds(15);

}  // namespace

TranslatorWindow::TranslatorWindow(const TRect& bounds, std::string script_path)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Translator", wnNoNumber),
      script_path_(std::move(script_path)) {
    const TRect inner = getExtent().grow(-1, -1);

    insert(new TLabel(TRect{inner.a.x, inner.a.y, inner.a.x + 7, inner.a.y + 1}, "Source:",
                      nullptr));
    source_lang_ = new TInputLine(
        TRect{inner.a.x + 8, inner.a.y, inner.a.x + 8 + kLangFieldMaxLen, inner.a.y + 1},
        kLangFieldMaxLen);
    insert(source_lang_);
    std::array<char, kLangFieldMaxLen + 1> src_buf{};
    std::strncpy(src_buf.data(), "AUTO", kLangFieldMaxLen);
    source_lang_->setData(src_buf.data());

    const int target_x = inner.a.x + 8 + kLangFieldMaxLen + 3;
    insert(new TLabel(TRect{target_x, inner.a.y, target_x + 7, inner.a.y + 1}, "Target:",
                      nullptr));
    target_lang_ = new TInputLine(
        TRect{target_x + 8, inner.a.y, target_x + 8 + kLangFieldMaxLen, inner.a.y + 1},
        kLangFieldMaxLen);
    insert(target_lang_);
    std::array<char, kLangFieldMaxLen + 1> tgt_buf{};
    std::strncpy(tgt_buf.data(), "EN", kLangFieldMaxLen);
    target_lang_->setData(tgt_buf.data());

    const int text_y = inner.a.y + 2;
    text_input_ = new TInputLine(TRect{inner.a.x, text_y, inner.b.x, text_y + 1}, kFieldMaxLen);
    insert(text_input_);

    const int btn_y = text_y + 2;
    constexpr int kBtnW = 14;
    const int btn_x = inner.a.x + (inner.b.x - inner.a.x - kBtnW) / 2;
    insert(new TButton(TRect{btn_x, btn_y, btn_x + kBtnW, btn_y + 2}, "~T~ranslate",
                       cmTranslateClick, bfDefault));

    const int result_y = btn_y + 3;
    result_ = new LabelView(TRect{inner.a.x, result_y, inner.b.x, inner.b.y});
    insert(result_);
    if (script_path_.empty()) {
        result_->set_text("error: translator-script not configured in config.toml");
    }

    text_input_->select();
}

void TranslatorWindow::start_translate() {
    if (script_path_.empty()) {
        result_->set_text("error: translator-script not configured in config.toml");
        return;
    }

    std::array<char, kLangFieldMaxLen + 1> src_buf{};
    source_lang_->getData(src_buf.data());
    std::array<char, kLangFieldMaxLen + 1> tgt_buf{};
    target_lang_->getData(tgt_buf.data());
    std::array<char, kFieldMaxLen + 1> text_buf{};
    text_input_->getData(text_buf.data());

    const std::string src(src_buf.data());
    const std::string tgt(tgt_buf.data());
    const std::string text(text_buf.data());
    if (text.empty()) {
        return;
    }

    // Fresh process per request -- tears down any still-running previous one
    // (SIGTERM via ExtensionProcess's destructor), matching "process exits"
    // per translate rather than a persistent chat-style REPL.
    process_ = std::make_unique<ExtensionProcess>(std::vector<std::string>{script_path_});
    const bool auto_source = src.empty() || src == "AUTO";
    const std::string line = auto_source ? (tgt + ": " + text) : (src + ">" + tgt + ": " + text);
    process_->write_line(line);
    awaiting_response_ = true;
    request_start_ = std::chrono::steady_clock::now();
    result_->set_text("translating...");
}

void TranslatorWindow::poll() {
    if (!awaiting_response_ || process_ == nullptr) {
        return;
    }
    const std::string chunk = process_->read_available();
    // translator.py's first line is its ready banner; skip it and wait for
    // the actual translation on the next line.
    const auto nl = chunk.find('\n');
    if (nl != std::string::npos) {
        std::string line = chunk.substr(0, nl);
        if (line.starts_with("translator ready")) {
            const auto second_nl = chunk.find('\n', nl + 1);
            if (second_nl != std::string::npos) {
                line = chunk.substr(nl + 1, second_nl - nl - 1);
            } else {
                line.clear();
            }
        }
        if (!line.empty()) {
            result_->set_text(line);
            awaiting_response_ = false;
            process_.reset();
            return;
        }
    }
    if (std::chrono::steady_clock::now() - request_start_ > kRequestTimeout) {
        result_->set_text("error: translation request timed out");
        awaiting_response_ = false;
        process_.reset();
    }
}

void TranslatorWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);

    if (event.what == evCommand &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.message.command == cmTranslateClick) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        start_translate();
        clearEvent(event);
        return;
    }
    if (event.what == evKeyDown &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.keyDown.keyCode == kbEnter) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        start_translate();
        clearEvent(event);
    }
}

void TranslatorWindow::reposition(const TRect& /*inner*/) {
    // Fixed-layout form -- v1 doesn't reflow on resize (same simplification
    // as ExtensionWindow's structured UI mode).
}

void TranslatorWindow::changeBounds(const TRect& bounds) {
    TWindow::changeBounds(bounds);
    reposition(getExtent().grow(-1, -1));
}

void TranslatorWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    // Fixed absolute form layout, no reflow on resize -- see
    // CalendarWindow::sizeLimits() for why this floor is needed.
    min.x = std::max(min.x, 38);
    min.y = std::max(min.y, 12);
}

TPalette& TranslatorWindow::getPalette() const {
    // cpGrayDialog -- see CalculatorWindow::getPalette() for why the default
    // (short) TWindow palette isn't enough for TButton/TInputLine.
    static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
