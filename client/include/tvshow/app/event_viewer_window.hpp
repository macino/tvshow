#pragma once

#define Uses_TWindow
#include <tvision/tv.h>

#include <string>
#include <vector>

namespace tvshow::app {

// adr-native-demo-windows: logs the raw TEvent structs this window receives
// (mouse/keyboard), newest last, scrollable -- inspired by magiblot/tvision's
// tvdemo screenshot. Scoped to events this specific window receives, not a
// whole-application event hook (the real tvdemo patches the event loop
// itself; that's a bigger change than this demo window warrants) -- move
// the mouse over it or type while it's focused to see events logged.
class EventViewerWindow : public TWindow {
public:
    explicit EventViewerWindow(const TRect& bounds);

    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

private:
    class EventLogViewer* viewer_{nullptr};  // non-owning; owned by TGroup
    std::vector<std::string> lines_;

    void log_event(const TEvent& event);
};

}  // namespace tvshow::app
