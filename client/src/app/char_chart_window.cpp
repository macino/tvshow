#include "tvshow/app/char_chart_window.hpp"

#define Uses_TButton
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#include <tvision/tv.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace tvshow::app {

namespace {

constexpr unsigned short cmPrevCharset = 5001;
constexpr unsigned short cmNextCharset = 5002;
constexpr int kBtnW = 5;
constexpr int kBtnRowH = 2;
constexpr int kChartY0 = kBtnRowH;  // chart starts right under the </> row

struct CharBlock {
    const char* name;
    char32_t first;
    char32_t last;
};

// Single-column-width blocks only -- tvision's CharGrid is one cell per
// column, and a double-width glyph (CJK, most emoji) would misalign every
// character after it on that row. Deliberately excludes those ranges.
constexpr std::array<CharBlock, 8> kBlocks = {{
    {"ASCII", 0x0020, 0x007E},
    {"Latin-1 Supplement", 0x00A0, 0x00FF},
    {"Latin Extended-A", 0x0100, 0x017F},
    {"Greek and Coptic", 0x0370, 0x03FF},
    {"Cyrillic", 0x0400, 0x04FF},
    {"General Punctuation", 0x2000, 0x206F},
    {"Box Drawing", 0x2500, 0x257F},
    {"Braille Patterns", 0x2800, 0x28FF},
}};

void utf8_encode(char32_t cp, std::string& out) {
    if (cp < 0x80U) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800U) {
        out += static_cast<char>(static_cast<uint8_t>(0xC0U | (cp >> 6U)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | (cp & 0x3FU)));
    } else {
        out += static_cast<char>(static_cast<uint8_t>(0xE0U | (cp >> 12U)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | ((cp >> 6U) & 0x3FU)));
        out += static_cast<char>(static_cast<uint8_t>(0x80U | (cp & 0x3FU)));
    }
}

}  // namespace

// Wraps to whatever width the window is -- one char per cell, no gaps
// (matches the reference screenshot's dense packed layout).
class ChartView : public TView {
public:
    ChartView(const TRect& bounds, CharChartWindow* win) : TView(bounds), owner_(win) {
        eventMask |= evMouseDown;
    }

    void set_block(const CharBlock& block) {
        block_ = block;
        drawView();
    }

    void draw() override {
        const int count = static_cast<int>(block_.last - block_.first) + 1;
        // Always repaint the full view height, even rows past this block's
        // last character -- otherwise switching from a larger block to a
        // smaller one leaves the previous block's glyphs on screen in the
        // rows this pass no longer touches.
        for (int row = 0; row < size.y; ++row) {
            std::string line;
            for (int col = 0; col < size.x; ++col) {
                const int idx = row * size.x + col;
                if (idx >= count) { break; }
                utf8_encode(block_.first + static_cast<char32_t>(idx), line);
            }
            TDrawBuffer buf;
            buf.moveChar(0, ' ', TColorAttr(0x07), static_cast<short>(size.x));
            buf.moveStr(0, line.c_str(), TColorAttr(0x07), static_cast<short>(size.x));
            writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
        }
    }

    void handleEvent(TEvent& event) override {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
            const TPoint local = makeLocal(event.mouse.where);  // NOLINT(cppcoreguidelines-pro-type-union-access)
            const int idx = local.y * size.x + local.x;
            const int count = static_cast<int>(block_.last - block_.first) + 1;
            if (idx >= 0 && idx < count) {
                owner_->show_char_info(block_.first + static_cast<char32_t>(idx));
            }
            clearEvent(event);
        }
    }

private:
    CharChartWindow* owner_;
    CharBlock block_ = kBlocks[0];
};

CharChartWindow::CharChartWindow(const TRect& bounds)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Char Chart", wnNoNumber) {
    const TRect inner = getExtent().grow(-1, -1);

    insert(new TButton(TRect{inner.a.x, inner.a.y, inner.a.x + kBtnW, inner.a.y + kBtnRowH}, "<",
                       cmPrevCharset, bfNormal));
    insert(new TButton(
        TRect{inner.b.x - kBtnW, inner.a.y, inner.b.x, inner.a.y + kBtnRowH}, ">",
        cmNextCharset, bfNormal));
    charset_label_ = new LabelView(
        TRect{inner.a.x + kBtnW + 1, inner.a.y, inner.b.x - kBtnW - 1, inner.a.y + 1},
        TColorAttr(0x07));
    insert(charset_label_);

    chart_ = new ChartView(TRect{inner.a.x, inner.a.y + kChartY0, inner.b.x, inner.b.y - 1}, this);
    insert(chart_);

    status_ = new LabelView(TRect{inner.a.x, inner.b.y - 1, inner.b.x, inner.b.y});
    insert(status_);
    status_->set_text("Click a character to see its Char/Decimal/Hex.");

    update_charset_label();
}

void CharChartWindow::update_charset_label() {
    charset_label_->set_text(kBlocks[charset_index_].name);
    static_cast<ChartView*>(chart_)->set_block(kBlocks[charset_index_]);
}

void CharChartWindow::next_charset() {
    charset_index_ = (charset_index_ + 1) % static_cast<int>(kBlocks.size());
    update_charset_label();
}

void CharChartWindow::prev_charset() {
    charset_index_ = (charset_index_ - 1 + static_cast<int>(kBlocks.size())) %
                      static_cast<int>(kBlocks.size());
    update_charset_label();
}

void CharChartWindow::show_char_info(char32_t cp) {
    std::string utf8;
    utf8_encode(cp, utf8);
    status_->set_text(
        std::format("Char: {}   Decimal: {}   Hex: {:#06x}", utf8, static_cast<uint32_t>(cp),
                   static_cast<uint32_t>(cp)));
}

void CharChartWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);
    if (event.what == evCommand) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        if (event.message.command == cmPrevCharset) {
            prev_charset();
            clearEvent(event);
        } else if (event.message.command == cmNextCharset) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
            next_charset();
            clearEvent(event);
        }
        return;
    }
    if (event.what == evKeyDown) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        const ushort code = event.keyDown.keyCode;  // NOLINT(cppcoreguidelines-pro-type-union-access)
        if (code == kbLeft) {
            prev_charset();
            clearEvent(event);
        } else if (code == kbRight) {
            next_charset();
            clearEvent(event);
        }
    }
}

void CharChartWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    min.x = std::max(min.x, 24);
    min.y = std::max(min.y, kChartY0 + 4 + 2);
}

TPalette& CharChartWindow::getPalette() const {
    // cpGrayDialog -- see CalculatorWindow::getPalette() for why the default
    // (short) TWindow palette isn't enough for TButton.
    static TPalette pal(cpGrayDialog, sizeof(cpGrayDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
