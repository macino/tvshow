#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tvshow::app {

struct Bookmark {
    std::string url;
    std::string title;  // may be empty — display falls back to url
};

// In-memory bookmark list. Pure: no I/O.
class BookmarkStore {
public:
    BookmarkStore() = default;

    // Parse the on-disk format: one entry per line, "url\ttitle" or just "url".
    // Blank lines and lines starting with '#' are ignored.
    static BookmarkStore parse(std::string_view content);

    // Serialise to the on-disk format.
    [[nodiscard]] std::string serialize() const;

    [[nodiscard]] const std::vector<Bookmark>& bookmarks() const noexcept { return bookmarks_; }
    [[nodiscard]] bool empty() const noexcept { return bookmarks_.empty(); }

    // Returns true if a bookmark with this exact URL already exists.
    [[nodiscard]] bool contains(std::string_view url) const noexcept;

    // Add a bookmark; no-op if URL already present.
    void add(std::string url, std::string title = "");

    // Remove bookmark by URL. No-op if not found.
    void remove(std::string_view url);

private:
    std::vector<Bookmark> bookmarks_;
};

// Path to the bookmarks file: ${XDG_CONFIG_HOME:-~/.config}/tvshow/bookmarks
[[nodiscard]] std::string bookmarks_default_path();

// Load bookmarks from path (or the default). Returns empty store on failure.
[[nodiscard]] BookmarkStore load_bookmarks(std::string_view path = "");

// Save bookmarks to path (or the default). Returns false on write failure.
[[nodiscard]] bool save_bookmarks(const BookmarkStore& store, std::string_view path = "");

}  // namespace tvshow::app
