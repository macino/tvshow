#include "tvshow/util/media_kind.hpp"

#include <doctest/doctest.h>

using tvshow::util::MediaKind;
using tvshow::util::classify_media_url;

TEST_CASE("classify_media_url: recognizes common video extensions") {
    CHECK(classify_media_url("http://example.com/clip.mp4") == MediaKind::Video);
    CHECK(classify_media_url("http://example.com/clip.webm") == MediaKind::Video);
    CHECK(classify_media_url("http://example.com/clip.MKV") == MediaKind::Video);
}

TEST_CASE("classify_media_url: recognizes common audio extensions") {
    CHECK(classify_media_url("http://example.com/song.mp3") == MediaKind::Audio);
    CHECK(classify_media_url("http://example.com/song.OGG") == MediaKind::Audio);
}

TEST_CASE("classify_media_url: ignores query string and fragment") {
    CHECK(classify_media_url("http://example.com/clip.mp4?t=10") == MediaKind::Video);
    CHECK(classify_media_url("http://example.com/clip.mp4#frag") == MediaKind::Video);
}

TEST_CASE("classify_media_url: plain HTML link is None") {
    CHECK(classify_media_url("http://example.com/page.html") == MediaKind::None);
    CHECK(classify_media_url("http://example.com/") == MediaKind::None);
}

// ── build_handler_argv ───────────────────────────────────────────────────────

using tvshow::util::build_handler_argv;

TEST_CASE("build_handler_argv: substitutes %s with the URL") {
    const auto argv = build_handler_argv("mpv %s", "http://example.com/clip.mp4");
    REQUIRE(argv.size() == 2);
    CHECK(argv[0] == "mpv");
    CHECK(argv[1] == "http://example.com/clip.mp4");
}

TEST_CASE("build_handler_argv: keeps flags before %s as separate argv entries") {
    const auto argv = build_handler_argv("mpv --no-video %s", "http://x/song.mp3");
    REQUIRE(argv.size() == 3);
    CHECK(argv[0] == "mpv");
    CHECK(argv[1] == "--no-video");
    CHECK(argv[2] == "http://x/song.mp3");
}

TEST_CASE("build_handler_argv: does not shell-interpolate a crafted URL") {
    const auto argv = build_handler_argv("mpv %s", "http://x/'; rm -rf ~ #.mp4");
    REQUIRE(argv.size() == 2);
    // The whole hostile string lands as ONE argv element, never parsed by a shell.
    CHECK(argv[1] == "http://x/'; rm -rf ~ #.mp4");
}

TEST_CASE("build_handler_argv: empty template yields empty argv") {
    CHECK(build_handler_argv("", "http://x/clip.mp4").empty());
}

TEST_CASE("build_handler_argv: collapses repeated whitespace") {
    const auto argv = build_handler_argv("mpv    %s", "u");
    REQUIRE(argv.size() == 2);
    CHECK(argv[0] == "mpv");
    CHECK(argv[1] == "u");
}
