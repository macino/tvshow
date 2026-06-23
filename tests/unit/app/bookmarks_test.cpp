#include "tvshow/app/bookmarks.hpp"

#include <doctest/doctest.h>

using tvshow::app::BookmarkStore;

TEST_CASE("BookmarkStore: empty on default construction") {
    const BookmarkStore store;
    CHECK(store.empty());
    CHECK(store.bookmarks().empty());
}

TEST_CASE("BookmarkStore: add and retrieve") {
    BookmarkStore store;
    store.add("http://example.com", "Example");
    REQUIRE(store.bookmarks().size() == 1);
    CHECK(store.bookmarks()[0].url == "http://example.com");
    CHECK(store.bookmarks()[0].title == "Example");
}

TEST_CASE("BookmarkStore: add ignores duplicate URL") {
    BookmarkStore store;
    store.add("http://example.com", "First");
    store.add("http://example.com", "Second");
    CHECK(store.bookmarks().size() == 1);
    CHECK(store.bookmarks()[0].title == "First");
}

TEST_CASE("BookmarkStore: add ignores empty URL") {
    BookmarkStore store;
    store.add("");
    CHECK(store.empty());
}

TEST_CASE("BookmarkStore: contains") {
    BookmarkStore store;
    store.add("http://example.com");
    CHECK(store.contains("http://example.com"));
    CHECK_FALSE(store.contains("http://other.com"));
}

TEST_CASE("BookmarkStore: remove by URL") {
    BookmarkStore store;
    store.add("http://a.com");
    store.add("http://b.com");
    store.remove("http://a.com");
    REQUIRE(store.bookmarks().size() == 1);
    CHECK(store.bookmarks()[0].url == "http://b.com");
}

TEST_CASE("BookmarkStore: remove non-existent is no-op") {
    BookmarkStore store;
    store.add("http://a.com");
    store.remove("http://nonexistent.com");
    CHECK(store.bookmarks().size() == 1);
}

TEST_CASE("BookmarkStore: parse — URL only") {
    const auto store = BookmarkStore::parse("http://example.com\n");
    REQUIRE(store.bookmarks().size() == 1);
    CHECK(store.bookmarks()[0].url == "http://example.com");
    CHECK(store.bookmarks()[0].title.empty());
}

TEST_CASE("BookmarkStore: parse — URL and title") {
    const auto store = BookmarkStore::parse("http://example.com\tExample Site\n");
    REQUIRE(store.bookmarks().size() == 1);
    CHECK(store.bookmarks()[0].url == "http://example.com");
    CHECK(store.bookmarks()[0].title == "Example Site");
}

TEST_CASE("BookmarkStore: parse — blank lines and comments ignored") {
    const auto store = BookmarkStore::parse(
        "# header\n"
        "\n"
        "http://a.com\n"
        "http://b.com\tB\n");
    CHECK(store.bookmarks().size() == 2);
}

TEST_CASE("BookmarkStore: serialize roundtrip") {
    BookmarkStore store;
    store.add("http://a.com", "A");
    store.add("http://b.com");
    const std::string s = store.serialize();
    const auto loaded = BookmarkStore::parse(s);
    REQUIRE(loaded.bookmarks().size() == 2);
    CHECK(loaded.bookmarks()[0].url == "http://a.com");
    CHECK(loaded.bookmarks()[0].title == "A");
    CHECK(loaded.bookmarks()[1].url == "http://b.com");
    CHECK(loaded.bookmarks()[1].title.empty());
}
