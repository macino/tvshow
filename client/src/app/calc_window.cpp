#include "tvshow/app/calc_window.hpp"

#include "tvshow/util/arith_eval.hpp"

#define Uses_TButton
#define Uses_TDialog
#define Uses_TEvent
#define Uses_TKeys
#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

namespace tvshow::app {

namespace {

constexpr unsigned short kKeyCmdBase = 3000;
constexpr int kColW = 6;
constexpr int kBtnW = 5;
constexpr int kBtnH = 2;
constexpr int kDisplayH = 1;  // matches reference screenshot's 1-row display
constexpr int kRowY0 = kDisplayH + 1;  // one blank row between display and keypad
constexpr int kDisplayW = 24;

// id, label, row, col -- same layout as the original window-provider
// calculator.py reference script (extensions/calculator/).
struct Key {
    int id;
    const char* label;
    int row;
    int col;
};
constexpr std::array<Key, 20> kKeys = {{
    {1, "C", 0, 0}, {2, "(", 0, 1}, {3, ")", 0, 2}, {4, "/", 0, 3},
    {5, "7", 1, 0}, {6, "8", 1, 1}, {7, "9", 1, 2}, {8, "*", 1, 3},
    {9, "4", 2, 0}, {10, "5", 2, 1}, {11, "6", 2, 2}, {12, "-", 2, 3},
    {13, "1", 3, 0}, {14, "2", 3, 1}, {15, "3", 3, 2}, {16, "+", 3, 3},
    {17, "0", 4, 0}, {18, ".", 4, 1}, {19, "<-", 4, 2}, {20, "=", 4, 3},
}};

}  // namespace

CalculatorWindow::CalculatorWindow(const TRect& bounds)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Calculator", wnNoNumber) {
    const TRect inner = getExtent().grow(-1, -1);

    // Blue bg, white fg -- matches the tvdemo reference screenshot. (An
    // earlier black/green "LCD" guess was wrong on two counts: it didn't
    // match the reference, and blue-on-the-window's-own-default-blue was
    // also invisible before `palette = wpGrayWindow` above fixed the
    // window's background to gray.)
    display_ = new LabelView(
        TRect{inner.a.x, inner.a.y, inner.a.x + kDisplayW, inner.a.y + kDisplayH},
        TColorAttr(0x1F));
    display_->set_right_align(true);
    insert(display_);

    for (const auto& key : kKeys) {
        const int x = inner.a.x + key.col * kColW;
        const int y = inner.a.y + kRowY0 + key.row * kBtnH;
        insert(new TButton(TRect{x, y, x + kBtnW, y + kBtnH}, key.label,
                           static_cast<unsigned short>(kKeyCmdBase + key.id), bfNormal));
    }

    update_display();
}

void CalculatorWindow::update_display() {
    display_->set_text(expr_.empty() ? "0" : expr_);
}

void CalculatorWindow::press(int key_id) {
    const auto it = std::find_if(kKeys.begin(), kKeys.end(),
                                 [key_id](const Key& k) { return k.id == key_id; });
    if (it == kKeys.end()) { return; }
    const std::string_view label = it->label;

    if (label == "C") {
        expr_.clear();
    } else if (label == "<-") {
        if (!expr_.empty()) { expr_.pop_back(); }
    } else if (label == "=") {
        if (const auto result = util::evaluate_arith(expr_)) {
            expr_ = std::format("{:g}", *result);
        } else {
            expr_ = "error";
        }
    } else {
        expr_ += label;
    }
    update_display();
}

void CalculatorWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);
    if (event.what == evCommand &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.message.command >= kKeyCmdBase &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.message.command < kKeyCmdBase + 100) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        press(static_cast<int>(event.message.command - kKeyCmdBase));  // NOLINT(cppcoreguidelines-pro-type-union-access)
        clearEvent(event);
        return;
    }
    if (event.what == evKeyDown) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const ushort code = event.keyDown.keyCode;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        int key_id = 0;
        if (code == kbEnter) {
            key_id = 20;  // "="
        } else if (code == kbBack) {
            key_id = 19;  // "<-"
        } else if (code == kbEsc) {
            key_id = 1;  // "C"
        } else {
            const char c = event.keyDown.charScan.charCode;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            if (c == 'c' || c == 'C') {
                key_id = 1;
            } else {
                const auto it = std::find_if(kKeys.begin(), kKeys.end(), [c](const Key& k) {
                    return k.label[0] == c && k.label[1] == '\0';
                });
                if (it != kKeys.end()) { key_id = it->id; }
            }
        }
        if (key_id != 0) {
            press(key_id);
            clearEvent(event);
        }
    }
}

void CalculatorWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    // See CalendarWindow::sizeLimits() -- fixed absolute keypad layout, no
    // reflow on resize, so shrinking below content size lets buttons poke
    // out past the frame instead of clipping gracefully.
    min.x = std::max(min.x, kDisplayW + 2);
    min.y = std::max(min.y, kRowY0 + 5 * kBtnH + 2);
}

TPalette& CalculatorWindow::getPalette() const {
    // cpGrayDialog (32 entries), not TWindow's wpGrayWindow (only 8) -- the
    // short window palette doesn't have enough entries for cpButton's
    // indices (up to 15), so TButton's colors fall through to a default
    // rather than picking up the gray-dialog theme. cpGrayDialog is what
    // TDialog itself uses and is long enough.
    static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
