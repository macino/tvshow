#pragma once

#include "tvshow/app/label_view.hpp"

#define Uses_TWindow
#include <tvision/tv.h>

namespace tvshow::app {

// adr-native-demo-windows: a reference chart of a Unicode block, plus a
// status line showing decimal/hex for whatever character the mouse is over.
// "<"/">" cycle through a fixed list of single-column-width blocks (no
// CJK/emoji/wide glyphs -- tvision's CharGrid is one cell = one column, and
// a double-width glyph in a single cell would misalign every character
// after it on that row). Clean-room implementation, ASCII table style
// inspired by magiblot/tvision's tvdemo screenshot; charset-cycling is new.
class CharChartWindow : public TWindow {
public:
    explicit CharChartWindow(const TRect& bounds);

    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

    // Called by ChartView on a click -- updates the status line.
    void show_char_info(char32_t cp);

    void next_charset();
    void prev_charset();

private:
    class ChartView* chart_{nullptr};  // non-owning; owned by TGroup
    LabelView* status_{nullptr};
    LabelView* charset_label_{nullptr};
    int charset_index_ = 0;

    void update_charset_label();
};

}  // namespace tvshow::app
