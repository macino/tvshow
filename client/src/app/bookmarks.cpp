#include "tvshow/app/bookmarks.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace tvshow::app {

namespace {

std::string_view trim_sv(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

BookmarkStore BookmarkStore::parse(std::string_view content) {
    BookmarkStore store;
    while (!content.empty()) {
        const auto nl = content.find('\n');
        const auto line = trim_sv(content.substr(0, nl));
        content = (nl == std::string_view::npos) ? "" : content.substr(nl + 1);

        if (line.empty() || line.front() == '#') { continue; }

        const auto tab = line.find('\t');
        if (tab == std::string_view::npos) {
            store.add(std::string(line));
        } else {
            store.add(std::string(line.substr(0, tab)),
                      std::string(trim_sv(line.substr(tab + 1))));
        }
    }
    return store;
}

std::string BookmarkStore::serialize() const {
    std::string out;
    for (const auto& bm : bookmarks_) {
        out += bm.url;
        if (!bm.title.empty()) {
            out += '\t';
            out += bm.title;
        }
        out += '\n';
    }
    return out;
}

bool BookmarkStore::contains(std::string_view url) const noexcept {
    return std::any_of(bookmarks_.begin(), bookmarks_.end(),
                       [url](const Bookmark& b) { return b.url == url; });
}

void BookmarkStore::add(std::string url, std::string title) {
    if (url.empty() || contains(url)) { return; }
    bookmarks_.push_back({std::move(url), std::move(title)});
}

void BookmarkStore::remove(std::string_view url) {
    bookmarks_.erase(
        std::remove_if(bookmarks_.begin(), bookmarks_.end(),
                       [url](const Bookmark& b) { return b.url == url; }),
        bookmarks_.end());
}

}  // namespace tvshow::app
