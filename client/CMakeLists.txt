# Pure-module library — no tvision, testable headlessly.
add_library(tvshow_core STATIC
    src/render/chargrid.cpp
    src/util/url.cpp
    src/net/http_client.cpp
    src/net/fake_http_client.cpp
    src/dom/node.cpp
    src/dom/parser.cpp
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
    PRIVATE
        tvshow_warnings
)

set_target_properties(tvshow_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Browser client — links tvision for TUI.
add_executable(tvshow
    src/main.cpp
    src/app/application.cpp
)

target_include_directories(tvshow
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tvshow
    PRIVATE
        tvshow_core
        tvision
        tvshow_warnings
)

set_target_properties(tvshow PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
