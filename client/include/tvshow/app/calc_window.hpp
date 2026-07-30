#pragma once

#include "tvshow/app/label_view.hpp"

#define Uses_TWindow
#include <tvision/tv.h>

#include <string>

namespace tvshow::app {

// adr-native-demo-windows: a native calculator (green keypad + blue
// display), clean-room implementation inspired by magiblot/tvision's
// tvdemo screenshot -- not a port of Borland's 1994 calc.cpp. Math is
// util::evaluate_arith (recursive-descent, no eval()-equivalent).
class CalculatorWindow : public TWindow {
public:
    explicit CalculatorWindow(const TRect& bounds);

    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

private:
    LabelView* display_{nullptr};
    std::string expr_;

    void press(int key_id);
    void update_display();
};

}  // namespace tvshow::app
