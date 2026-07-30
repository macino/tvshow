# Pure-module library — no tvision, testable headlessly.
add_library(tvshow_core STATIC
    src/render/chargrid.cpp
    src/util/url.cpp
    src/util/charset.cpp
    src/util/config.cpp
    src/util/media_kind.cpp
    src/util/arith_eval.cpp
    src/util/month_grid.cpp
    src/app/bookmarks.cpp
    src/app/extension_ui_protocol.cpp
    src/net/extension_manifest.cpp
    src/net/cookie_jar.cpp
    src/script/lua_engine.cpp
    src/net/blocklist.cpp
    src/net/request_log.cpp
    src/net/http_client.cpp
    src/net/fake_http_client.cpp
    src/dom/node.cpp
    src/dom/parser.cpp
    src/css/parser.cpp
    src/style/resolver.cpp
    src/layout/engine.cpp
    src/layout/form.cpp
    src/layout/form_data.cpp
    src/layout/form_focus.cpp
    src/layout/inline_text.cpp
    src/layout/links.cpp
    src/render/render.cpp
    src/images/alt_text_renderer.cpp
    src/images/braille_renderer.cpp
    src/images/ascii_art_renderer.cpp
    src/images/decode.cpp
    src/images/stb_image_impl.cpp
)

target_include_directories(tvshow_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# SYSTEM: stb_image.h is vendored third-party code (ADR-004) that isn't
# warning-clean under our -Werror flags; treating it as a system header
# suppresses warnings originating there without loosening our own code.
target_include_directories(tvshow_core SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

find_library(ICONV_LIB iconv)

target_link_libraries(tvshow_core
    PUBLIC
        gumbo::gumbo
        katana::katana
        lua::lua
    PRIVATE
        tvshow_warnings
        $<$<BOOL:${ICONV_LIB}>:${ICONV_LIB}>
)

set_target_properties(tvshow_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# paint: CharGrid -> tvision TDrawBuffer. Split from tvshow_core so the pure
# library stays tvision-free; tests link this directly to exercise it
# headlessly (constructing a TDrawBuffer needs no real terminal).
add_library(tvshow_paint STATIC
    src/paint/paint.cpp
)

target_include_directories(tvshow_paint
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tvshow_paint
    PUBLIC
        tvshow_core
        tvision
    PRIVATE
        tvshow_warnings
)

set_target_properties(tvshow_paint PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# tvshow_io: page loading (file I/O, HTTP fetch). No tvision dependency,
# so it's also linked directly by tests/integration without dragging in
# the TUI. Kept out of tvshow_core because it does real I/O.
add_library(tvshow_io STATIC
    src/app/page.cpp
    src/app/bookmarks_io.cpp
    src/net/cpp_http_client.cpp
    src/net/cookie_jar_io.cpp
    src/net/blocklist_io.cpp
    src/util/log.cpp
    src/util/config_io.cpp
)

target_include_directories(tvshow_io
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tvshow_io
    PUBLIC
        tvshow_core
    PRIVATE
        httplib::httplib
        Threads::Threads
        tvshow_warnings
)

set_target_properties(tvshow_io PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Headless renderer — captures page render output as plain text. No tvision.
add_executable(tvshow-capture
    src/capture_main.cpp
)

target_link_libraries(tvshow-capture
    PRIVATE
        tvshow_io
        tvshow_warnings
)

set_target_properties(tvshow-capture PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Browser client — links tvision for TUI.
add_executable(tvshow
    src/main.cpp
    src/app/application.cpp
    src/app/browser_view.cpp
    src/app/browser_window.cpp
    src/app/builtin_themes.cpp
    src/app/settings_dialog.cpp
    src/app/external_handler.cpp
    src/app/extension_process.cpp
    src/app/extension_window.cpp
    src/net/extension_server.cpp
    src/app/label_view.cpp
    src/app/translator_window.cpp
    src/app/calc_window.cpp
    src/app/calendar_window.cpp
    src/app/puzzle_window.cpp
    src/app/char_chart_window.cpp
    src/app/event_viewer_window.cpp
)

target_include_directories(tvshow
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(tvshow PRIVATE
    TVSHOW_EXTENSIONS_DIR="${CMAKE_SOURCE_DIR}/extensions"
)

target_link_libraries(tvshow
    PRIVATE
        tvshow_core
        tvshow_paint
        tvshow_io
        tvision
        httplib::httplib
        Threads::Threads
        tvshow_warnings
)

set_target_properties(tvshow PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
