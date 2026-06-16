#pragma once

#define Uses_TView
#include "tvshow/render/chargrid.hpp"

#include <tvision/tv.h>

namespace tvshow::app {

// Hosts one already-rendered CharGrid (the M8 scope: a static page, no
// navigation/history yet — that lands with the net+layout wiring in later
// milestones).
class BrowserView : public TView {
public:
    BrowserView(const TRect& bounds, render::CharGrid grid);

    void draw() override;

private:
    render::CharGrid grid_;
};

}  // namespace tvshow::app
