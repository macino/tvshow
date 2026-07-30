# Download the single-header cpp-httplib and expose it as an INTERFACE
# target. Same rationale as Doctest.cmake: a header-only dep doesn't need
# its own build, just a pinned download. See docs/decisions/001.

set(TVSHOW_HTTPLIB_TAG "v0.18.3" CACHE STRING "cpp-httplib version tag")

# Packaging override (e.g. a Homebrew Formula's `resource` block, staged
# ahead of time so the sandboxed build step needs no network access): point
# this at a directory that already contains httplib.h instead of
# downloading it. See docs/packaging.md.
set(TVSHOW_HTTPLIB_HEADER_DIR "" CACHE PATH
    "Directory already containing httplib.h; skips the network fetch when set")

if(TVSHOW_HTTPLIB_HEADER_DIR)
    set(_httplib_include_dir "${TVSHOW_HTTPLIB_HEADER_DIR}")
else()
    set(_httplib_include_dir "${CMAKE_BINARY_DIR}/_httplib_include")
    set(_httplib_header "${_httplib_include_dir}/httplib.h")
    if(NOT EXISTS "${_httplib_header}")
        message(STATUS "Downloading cpp-httplib ${TVSHOW_HTTPLIB_TAG} header...")
        file(
            DOWNLOAD
            "https://raw.githubusercontent.com/yhirose/cpp-httplib/${TVSHOW_HTTPLIB_TAG}/httplib.h"
            "${_httplib_header}"
            TLS_VERIFY ON
        )
    endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_library(httplib_iface INTERFACE)
add_library(httplib::httplib ALIAS httplib_iface)
target_include_directories(httplib_iface INTERFACE "${_httplib_include_dir}")
target_compile_definitions(httplib_iface INTERFACE
    CPPHTTPLIB_OPENSSL_SUPPORT
    CPPHTTPLIB_ZLIB_SUPPORT)
target_link_libraries(httplib_iface INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
