cmake_minimum_required(VERSION 3.25)
project(tvshow LANGUAGES C CXX VERSION 0.0.0)

option(TVSHOW_WARNINGS_AS_ERRORS "Treat warnings as errors on tvshow targets" ON)
set(TVSHOW_SANITIZE "" CACHE STRING "Sanitizers to enable on tvshow targets (semicolon list)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(Warnings)
include(Tvision)
include(Gumbo)
include(Doctest)

enable_testing()

add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(tests)

message(STATUS "tvshow configured: build=${CMAKE_BUILD_TYPE}, sanitize=${TVSHOW_SANITIZE}, werror=${TVSHOW_WARNINGS_AS_ERRORS}")
