# Download the single-header doctest and expose it as an INTERFACE target.
# Avoids doctest's own CMakeLists.txt which requires CMake < 3.5 (dropped in CMake 4.x).

set(TVSHOW_DOCTEST_TAG "v2.4.11" CACHE STRING "doctest version tag")

# Packaging override (e.g. a Homebrew Formula's `resource` block, staged
# ahead of time so the sandboxed build step needs no network access): point
# this at a directory that already contains doctest/doctest.h instead of
# downloading it. See docs/packaging.md.
set(TVSHOW_DOCTEST_HEADER_DIR "" CACHE PATH
    "Directory already containing doctest/doctest.h; skips the network fetch when set")

if(TVSHOW_DOCTEST_HEADER_DIR)
    set(_doctest_include_dir "${TVSHOW_DOCTEST_HEADER_DIR}")
else()
    set(_doctest_include_dir "${CMAKE_BINARY_DIR}/_doctest_include")
    set(_doctest_header "${_doctest_include_dir}/doctest/doctest.h")
    if(NOT EXISTS "${_doctest_header}")
        message(STATUS "Downloading doctest ${TVSHOW_DOCTEST_TAG} header...")
        file(
            DOWNLOAD
            "https://raw.githubusercontent.com/doctest/doctest/${TVSHOW_DOCTEST_TAG}/doctest/doctest.h"
            "${_doctest_header}"
            TLS_VERIFY ON
        )
    endif()
endif()

add_library(doctest_iface INTERFACE)
add_library(doctest::doctest ALIAS doctest_iface)
target_include_directories(doctest_iface INTERFACE "${_doctest_include_dir}")
