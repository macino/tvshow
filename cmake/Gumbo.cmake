# Fetch google/gumbo-parser (pure C HTML5 parser) and expose gumbo::gumbo.
# Gumbo has no CMakeLists.txt, so sources are compiled as part of this project.

include(FetchContent)

set(TVSHOW_GUMBO_TAG "v0.10.1"
    CACHE STRING "gumbo-parser tag to pin")

FetchContent_Declare(
    gumbo_parser
    GIT_REPOSITORY https://github.com/google/gumbo-parser.git
    GIT_TAG        ${TVSHOW_GUMBO_TAG}
    GIT_SHALLOW    TRUE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(gumbo_parser)
if(NOT gumbo_parser_POPULATED)
    FetchContent_Populate(gumbo_parser)

    file(GLOB _gumbo_sources "${gumbo_parser_SOURCE_DIR}/src/*.c")

    add_library(gumbo_lib STATIC ${_gumbo_sources})
    set_target_properties(gumbo_lib PROPERTIES
        C_STANDARD 99
        SYSTEM TRUE
    )
    target_include_directories(gumbo_lib SYSTEM
        PUBLIC "${gumbo_parser_SOURCE_DIR}/src"
    )
    add_library(gumbo::gumbo ALIAS gumbo_lib)
endif()
