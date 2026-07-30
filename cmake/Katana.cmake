# Fetch hackers-painters/katana-parser (CSS3 grammar) and expose katana::katana.
# Katana has no CMakeLists.txt; sources are compiled as part of this project.

include(FetchContent)

set(TVSHOW_KATANA_TAG "499118d32c387a893fdc9dda2cb95eee524bdb9b"
    CACHE STRING "katana-parser commit to pin")

# Packaging override (e.g. a Homebrew Formula's `resource` block, staged
# ahead of time so the sandboxed build step needs no network access): point
# this at an already-checked-out, **already-patched** katana-parser tree
# (the six patches below are applied via PATCH_COMMAND on the normal fetch
# path; SOURCE_DIR skips FetchContent's update step entirely, so a packager
# using this override is responsible for applying cmake/patches/katana-*.cmake
# themselves before staging the resource -- see docs/packaging.md).
set(TVSHOW_KATANA_SOURCE_DIR "" CACHE PATH
    "Pre-staged, pre-patched katana-parser source dir; skips the network fetch when set")

# Five upstream bugs patched via cmake/patches/:
#   katana-fix-chs-unit.cmake            — tokenizer.c never parses `ch`-unit values.
#   katana-null-data-ptr.cmake           — destroy loop crashes on NULL array->data.
#   katana-error-scan-text.cmake         — katanaerror() crashes on garbage scanner text.
#   katana-array-destroy-null.cmake      — katana_array_destroy() leaves dangling data
#                                          pointer after free; zeroing it here closes the
#                                          use-after-free that caused SIGSEGV on real-world
#                                          CSS with modern properties (user-select, etc.).
#   katana-destroy-style-rule-guard.cmake — katana_destroy_style_rule() lacks guards for
#                                           uninitialised/corrupted selectors+declarations
#                                           arrays; triggers assert/double-free on
#                                           error-recovery rules from modern CSS.
#   katana-dedup-rules.cmake              — error-recovery adds the same KatanaRule* twice
#                                           to rules arrays; dedup before destroy to prevent
#                                           double-free in katana_destroy_stylesheet and
#                                           katana_destroy_rule_list (@media child rules).
if(TVSHOW_KATANA_SOURCE_DIR)
    FetchContent_Declare(
        katana_parser
        SOURCE_DIR "${TVSHOW_KATANA_SOURCE_DIR}"
    )
else()
    FetchContent_Declare(
        katana_parser
        GIT_REPOSITORY https://github.com/hackers-painters/katana-parser.git
        GIT_TAG        ${TVSHOW_KATANA_TAG}
        GIT_SHALLOW    FALSE
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND  ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-fix-chs-unit.cmake
            COMMAND    ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-null-data-ptr.cmake
            COMMAND    ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-error-scan-text.cmake
            COMMAND    ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-array-destroy-null.cmake
            COMMAND    ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-destroy-style-rule-guard.cmake
            COMMAND    ${CMAKE_COMMAND} -DKATANA_SRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/katana-dedup-rules.cmake
    )
endif()

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
