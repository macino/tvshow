# Download the single-header doctest and expose it as an INTERFACE target.
# Avoids doctest's own CMakeLists.txt which requires CMake < 3.5 (dropped in CMake 4.x).

set(TVSHOW_DOCTEST_TAG "v2.4.11" CACHE STRING "doctest version tag")
set(_doctest_header "${CMAKE_BINARY_DIR}/_doctest_include/doctest/doctest.h")

if(NOT EXISTS "${_doctest_header}")
    message(STATUS "Downloading doctest ${TVSHOW_DOCTEST_TAG} header...")
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/doctest/doctest/${TVSHOW_DOCTEST_TAG}/doctest/doctest.h"
        "${_doctest_header}"
        TLS_VERIFY ON
    )
endif()

add_library(doctest_iface INTERFACE)
add_library(doctest::doctest ALIAS doctest_iface)
target_include_directories(doctest_iface INTERFACE "${CMAKE_BINARY_DIR}/_doctest_include")
