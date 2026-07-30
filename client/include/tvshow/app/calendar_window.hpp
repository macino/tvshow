#pragma once

#include "tvshow/app/label_view.hpp"

#define Uses_TWindow
#define Uses_TInputLine
#include <tvision/tv.h>

namespace tvshow::app {

// adr-native-demo-windows: a native calendar (month grid, "<"/">" to
// navigate), clean-room implementation inspired by magiblot/tvision's
// tvdemo screenshot. Grid text comes from util::format_month_grid.
class CalendarWindow : public TWindow {
public:
    explicit CalendarWindow(const TRect& bounds);

    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

private:
    LabelView* grid_{nullptr};
    TInputLine* year_input_{nullptr};
    int year_;
    int month_;

    void update_grid();
};

}  // namespace tvshow::app
