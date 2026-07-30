#pragma once

#include "tvshow/app/label_view.hpp"

#define Uses_TButton
#define Uses_TWindow
#include <tvision/tv.h>

#include <array>
#include <string>

namespace tvshow::app {

// adr-native-demo-windows: a native 15-puzzle (4x4 sliding tiles, A..O +
// blank), clean-room implementation inspired by magiblot/tvision's tvdemo
// screenshot -- not a port of Borland's 1994 puzzle.cpp.
class PuzzleWindow : public TWindow {
public:
    explicit PuzzleWindow(const TRect& bounds);

    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

private:
    static constexpr int kSize = 4;
    static constexpr int kTiles = kSize * kSize;

    std::array<int, kTiles> cells_{};   // 0 = blank, 1..15 = letters A..O
    std::array<std::string, kTiles> labels_;
    std::array<TButton*, kTiles> buttons_{};
    int blank_pos_ = kTiles - 1;
    int moves_ = 0;
    LabelView* moves_label_{nullptr};

    void shuffle();
    void relabel();
    void try_move(int pos);
    [[nodiscard]] bool solved() const noexcept;
};

}  // namespace tvshow::app
