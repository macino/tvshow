#include "tvshow/app/puzzle_window.hpp"

#define Uses_TDialog
#define Uses_TEvent
#include <tvision/tv.h>

#include <algorithm>
#include <cstdlib>
#include <format>
#include <random>

namespace tvshow::app {

namespace {
constexpr unsigned short kTileCmdBase = 5000;
constexpr int kCellW = 5;
constexpr int kCellH = 2;
constexpr int kGridY0 = 2;
}  // namespace

PuzzleWindow::PuzzleWindow(const TRect& bounds)
    : TWindowInit(&TWindow::initFrame), TWindow(bounds, "Puzzle", wnNoNumber) {
    for (int i = 0; i < kTiles; ++i) { cells_[static_cast<size_t>(i)] = i + 1; }
    cells_[kTiles - 1] = 0;
    blank_pos_ = kTiles - 1;

    const TRect inner = getExtent().grow(-1, -1);
    for (int pos = 0; pos < kTiles; ++pos) {
        const int row = pos / kSize;
        const int col = pos % kSize;
        const int x = inner.a.x + col * kCellW;
        const int y = inner.a.y + kGridY0 + row * kCellH;
        auto* btn = new TButton(TRect{x, y, x + kCellW, y + kCellH}, "",
                                static_cast<unsigned short>(kTileCmdBase + pos), bfNormal);
        insert(btn);
        buttons_[static_cast<size_t>(pos)] = btn;
    }

    // Blue bg matching the window's own default (wpBlueWindow) -- plain
    // inline-looking text instead of a separate black box, closer to the
    // reference's plain "Move 0" label.
    moves_label_ = new LabelView(TRect{inner.a.x, inner.a.y, inner.a.x + 20, inner.a.y + 1},
                                 TColorAttr(0x1F));
    insert(moves_label_);

    shuffle();
    relabel();
}

void PuzzleWindow::shuffle() {
    std::mt19937 rng(std::random_device{}());
    constexpr int kShuffleMoves = 200;
    for (int i = 0; i < kShuffleMoves; ++i) {
        std::array<int, 4> neighbors{-1, -1, -1, -1};
        int n = 0;
        const int br = blank_pos_ / kSize;
        const int bc = blank_pos_ % kSize;
        if (br > 0) { neighbors[static_cast<size_t>(n++)] = blank_pos_ - kSize; }
        if (br < kSize - 1) { neighbors[static_cast<size_t>(n++)] = blank_pos_ + kSize; }
        if (bc > 0) { neighbors[static_cast<size_t>(n++)] = blank_pos_ - 1; }
        if (bc < kSize - 1) { neighbors[static_cast<size_t>(n++)] = blank_pos_ + 1; }
        std::uniform_int_distribution<int> dist(0, n - 1);
        const int swap_pos = neighbors[static_cast<size_t>(dist(rng))];
        std::swap(cells_[static_cast<size_t>(blank_pos_)], cells_[static_cast<size_t>(swap_pos)]);
        blank_pos_ = swap_pos;
    }
    moves_ = 0;  // shuffling doesn't count as player moves
}

void PuzzleWindow::relabel() {
    for (int pos = 0; pos < kTiles; ++pos) {
        const int v = cells_[static_cast<size_t>(pos)];
        labels_[static_cast<size_t>(pos)] = (v == 0) ? "" : std::string(1, static_cast<char>('A' + v - 1));
        TButton* btn = buttons_[static_cast<size_t>(pos)];
        // TButton::~TButton() unconditionally `delete[]`s title (it's
        // always newStr()-allocated by the constructor) -- pointing title
        // straight at labels_[pos].c_str() (a std::string's own buffer,
        // never newStr'd, and liable to move on the very next relabel()
        // call) is a use-after-free/invalid-free waiting to happen, which
        // is exactly what surfaced as "free(): invalid pointer" on window
        // close. Match the constructor's allocation: delete the old
        // newStr'd buffer, then newStr a fresh one.
        delete[] const_cast<char*>(btn->title);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
        btn->title = newStr(labels_[static_cast<size_t>(pos)]);
        btn->drawView();
    }
    moves_label_->set_text(solved() ? std::format("Moves: {} - Solved!", moves_)
                                    : std::format("Moves: {}", moves_));
}

bool PuzzleWindow::solved() const noexcept {
    for (int i = 0; i < kTiles - 1; ++i) {
        if (cells_[static_cast<size_t>(i)] != i + 1) { return false; }
    }
    return cells_[kTiles - 1] == 0;
}

void PuzzleWindow::try_move(int pos) {
    const int pr = pos / kSize;
    const int pc = pos % kSize;
    const int br = blank_pos_ / kSize;
    const int bc = blank_pos_ % kSize;
    const bool adjacent = (pr == br && std::abs(pc - bc) == 1) ||
                         (pc == bc && std::abs(pr - br) == 1);
    if (!adjacent) { return; }
    std::swap(cells_[static_cast<size_t>(pos)], cells_[static_cast<size_t>(blank_pos_)]);
    blank_pos_ = pos;
    ++moves_;
    relabel();
}

void PuzzleWindow::handleEvent(TEvent& event) {
    TWindow::handleEvent(event);
    if (event.what == evCommand &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.message.command >= kTileCmdBase &&  // NOLINT(cppcoreguidelines-pro-type-union-access)
        event.message.command < kTileCmdBase + kTiles) {  // NOLINT(cppcoreguidelines-pro-type-union-access)
        try_move(static_cast<int>(event.message.command - kTileCmdBase));  // NOLINT(cppcoreguidelines-pro-type-union-access)
        clearEvent(event);
    }
}

void PuzzleWindow::sizeLimits(TPoint& min, TPoint& max) {
    TWindow::sizeLimits(min, max);
    // See CalendarWindow::sizeLimits() -- fixed absolute tile grid, no
    // reflow on resize.
    min.x = std::max(min.x, kSize * kCellW + 2);
    min.y = std::max(min.y, kGridY0 + kSize * kCellH + 2);
}

TPalette& PuzzleWindow::getPalette() const {
    // cpBlueDialog, not the default wpBlueWindow -- see
    // CalculatorWindow::getPalette() for why the short window palette isn't
    // long enough for TButton (16 tiles, all TButtons here).
    static TPalette pal(cpBlueDialog, sizeof(cpBlueDialog) - 1);
    return pal;
}

}  // namespace tvshow::app
