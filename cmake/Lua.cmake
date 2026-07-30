# Fetch lua/lua (Lua interpreter, MIT) and expose lua::lua. Like gumbo/katana,
# upstream has no CMakeLists.txt; sources are compiled as part of this project.
#
# adr-sandboxed-scripting: embedded, sandboxed per client-side script call
# (client/src/script/lua_engine.cpp) -- io/os/package/debug are never linked
# in via luaL_openlibs; only base/string/math/table are opened explicitly.

include(FetchContent)

set(TVSHOW_LUA_TAG "v5.4.7"
    CACHE STRING "lua/lua tag to pin")

FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        ${TVSHOW_LUA_TAG}
    GIT_SHALLOW    TRUE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)

    file(GLOB _lua_sources "${lua_SOURCE_DIR}/*.c")
    # lua.c is the standalone `lua` CLI's main() -- not wanted in the embedded
    # library. ltests.c is Lua's own internal test harness (debug-only
    # globals, never wanted in an embed). onelua.c is the alternative
    # "everything in one translation unit" build (#includes lapi.c,
    # lauxlib.c, etc. as text, and also defines main() by default) -- built
    # *instead of* the individual files, not alongside them; compiling it
    # alongside the per-file sources below silently double-defines every
    # Lua API symbol in the resulting .a (ar doesn't resolve symbols, so
    # this doesn't fail until something actually links against it).
    list(FILTER _lua_sources EXCLUDE REGEX ".*/(lua|ltests|onelua)\\.c$")

    add_library(lua_lib STATIC ${_lua_sources})
    set_target_properties(lua_lib PROPERTIES
        C_STANDARD 99
        SYSTEM TRUE
    )
    target_include_directories(lua_lib SYSTEM
        PUBLIC "${lua_SOURCE_DIR}"
    )
    # Lua expects a POSIX-ish target by default; LUA_USE_LINUX pulls in
    # readline/dl for the standalone CLI we don't build -- LUA_USE_POSIX is
    # the narrower "just the interpreter core" definition.
    target_compile_definitions(lua_lib PRIVATE LUA_USE_POSIX)
    add_library(lua::lua ALIAS lua_lib)
endif()
