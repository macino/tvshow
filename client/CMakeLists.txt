# Pure-module library — no tvision, testable headlessly.
add_library(tvshow_core STATIC
    src/render/chargrid.cpp
    src/util/url.cpp
    src/net/http_client.cpp
    src/net/fake_http_client.cpp
    src/dom/node.cpp
    src/dom/parser.cpp
    src/css/parser.cpp
    src/style/resolver.cpp
    src/layout/engine.cpp
    src/render/render.cpp
)

target_include_directories(tvshow_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tvshow_core
    PUBLIC
        gumbo::gumbo
        katana::katana
    PRIVATE
        tvshow_warnings
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

# Browser client — links tvision for TUI.
add_executable(tvshow
    src/main.cpp
    src/app/application.cpp
    src/app/browser_view.cpp
)

target_include_directories(tvshow
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tvshow
    PRIVATE
        tvshow_core
        tvshow_paint
        tvision
        tvshow_warnings
)

set_target_properties(tvshow PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
