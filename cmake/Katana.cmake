# Fetch hackers-painters/katana-parser (CSS3 grammar) and expose katana::katana.
# Katana has no CMakeLists.txt; sources are compiled as part of this project.

include(FetchContent)

set(TVSHOW_KATANA_TAG "499118d32c387a893fdc9dda2cb95eee524bdb9b"
    CACHE STRING "katana-parser commit to pin")

# Upstream bug: tokenizer.c never parses the numeric value of `ch`-unit
# lengths (missing switch case) — see cmake/patches/katana-fix-chs-unit.cmake.
FetchContent_Declare(
    katana_parser
    GIT_REPOSITORY https://github.com/hackers-painters/katana-parser.git
    GIT_TAG        ${TVSHOW_KATANA_TAG}
    GIT_SHALLOW    FALSE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-fix-chs-unit.cmake
)

FetchContent_GetProperties(katana_parser)
if(NOT katana_parser_POPULATED)
    FetchContent_Populate(katana_parser)

    file(GLOB _katana_sources "${katana_parser_SOURCE_DIR}/src/*.c")

    add_library(katana_lib STATIC ${_katana_sources})
    set_target_properties(katana_lib PROPERTIES
        C_STANDARD 99
        SYSTEM TRUE
    )
    target_include_directories(katana_lib SYSTEM
        PUBLIC "${katana_parser_SOURCE_DIR}/src"
    )
    add_library(katana::katana ALIAS katana_lib)
endif()
