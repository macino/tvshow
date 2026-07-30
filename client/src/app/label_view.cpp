#include "tvshow/app/label_view.hpp"

#define Uses_TDrawBuffer
#include <tvision/tv.h>

#include <algorithm>
#include <sstream>
#include <string>

namespace tvshow::app {

void LabelView::draw() {
    std::istringstream in(text_);
    std::string line;
    int row = 0;
    while (row < size.y) {
        const bool has_line = static_cast<bool>(std::getline(in, line));
        TDrawBuffer buf;
        buf.moveChar(0, ' ', attr_, static_cast<short>(size.x));
        if (has_line) {
            if (right_align_) {
                const int start = std::max(0, size.x - static_cast<int>(line.size()));
                buf.moveStr(static_cast<short>(start), line.c_str(), attr_,
                           static_cast<short>(size.x - start));
            } else {
                buf.moveStr(0, line.c_str(), attr_, static_cast<short>(size.x));
            }
        }
        writeLine(0, static_cast<short>(row), static_cast<short>(size.x), 1, buf);
        ++row;
    }
}

}  // namespace tvshow::app
