# Download the single-header cpp-httplib and expose it as an INTERFACE
# target. Same rationale as Doctest.cmake: a header-only dep doesn't need
# its own build, just a pinned download. See docs/decisions/001.

set(TVSHOW_HTTPLIB_TAG "v0.18.3" CACHE STRING "cpp-httplib version tag")
set(_httplib_header "${CMAKE_BINARY_DIR}/_httplib_include/httplib.h")

if(NOT EXISTS "${_httplib_header}")
    message(STATUS "Downloading cpp-httplib ${TVSHOW_HTTPLIB_TAG} header...")
    file(
        DOWNLOAD
        "https://raw.githubusercontent.com/yhirose/cpp-httplib/${TVSHOW_HTTPLIB_TAG}/httplib.h"
        "${_httplib_header}"
        TLS_VERIFY ON
    )
endif()

find_package(OpenSSL REQUIRED)

add_library(httplib_iface INTERFACE)
add_library(httplib::httplib ALIAS httplib_iface)
target_include_directories(httplib_iface INTERFACE "${CMAKE_BINARY_DIR}/_httplib_include")
target_compile_definitions(httplib_iface INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib_iface INTERFACE OpenSSL::SSL OpenSSL::Crypto)
