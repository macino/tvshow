#pragma once

#define Uses_TView
#include <tvision/tv.h>

#include <string>

namespace tvshow::app {

// Read-only, post-construction-updatable text view. TStaticText can't have
// its text changed after construction, which both ExtensionWindow's
// UI-protocol TEXT command and TranslatorWindow's result field need — this
// is the minimal replacement: set_text() + redraw.
class LabelView : public TView {
public:
    explicit LabelView(const TRect& bounds, TColorAttr attr = TColorAttr(0x07))
        : TView(bounds), attr_(attr) {}

    void set_text(std::string text) {
        text_ = std::move(text);
        drawView();
    }

    // Calculator-style display: pads/truncates each line to the right edge
    // instead of the left. Off by default (matches TEXT/ScrollbackViewer's
    // left-aligned use elsewhere).
    void set_right_align(bool on) { right_align_ = on; }

    void draw() override;

private:
    std::string text_;
    TColorAttr attr_;
    bool right_align_ = false;
};

}  // namespace tvshow::app
