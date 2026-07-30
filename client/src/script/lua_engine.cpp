#include "tvshow/script/lua_engine.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <string>
#include <utility>

namespace tvshow::script {

namespace {

// Generous enough for real button logic, low enough that a runaway loop
// aborts near-instantly instead of hanging the render pipeline.
constexpr int kInstructionBudget = 200000;

void count_hook(lua_State* L, lua_Debug* /*ar*/) {
    luaL_error(L, "script exceeded instruction budget (%d)", kInstructionBudget);
}

// Opens only base/string/math/table, then strips load-new-code entry
// points from base. No io/os/package/debug -- never linked in at all, so
// there's no global to strip; the capability doesn't exist to reach.
void open_sandboxed_libs(lua_State* L) {
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    const char* const kStripped[] = {"load", "loadstring", "dofile", "loadfile", "require"};
    for (const char* name : kStripped) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
}

dom::Document* doc_upvalue(lua_State* L) {
    return static_cast<dom::Document*>(lua_touserdata(L, lua_upvalueindex(1)));
}

int lua_set_text(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);
    dom::Document* doc = doc_upvalue(L);
    if (dom::Node* node = dom::find_by_id(*doc->root, id)) {
        for (const auto& child : node->children) {
            if (child && child->kind == dom::NodeKind::Text) {
                child->text = text;
                break;
            }
        }
    }
    // Unknown id: no-op, not an error -- degrade gracefully, matching the
    // rest of this codebase's stance on missing-target situations.
    return 0;
}

int lua_get_text(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    dom::Document* doc = doc_upvalue(L);
    if (const dom::Node* node = dom::find_by_id(*doc->root, id)) {
        for (const auto& child : node->children) {
            if (child && child->kind == dom::NodeKind::Text) {
                lua_pushstring(L, child->text.c_str());
                return 1;
            }
        }
    }
    lua_pushstring(L, "");
    return 1;
}

void register_tv_api(lua_State* L, dom::Document& doc) {
    lua_newtable(L);  // tv

    lua_pushlightuserdata(L, &doc);
    lua_pushcclosure(L, lua_set_text, 1);
    lua_setfield(L, -2, "set_text");

    lua_pushlightuserdata(L, &doc);
    lua_pushcclosure(L, lua_get_text, 1);
    lua_setfield(L, -2, "get_text");

    lua_setglobal(L, "tv");
}

}  // namespace

LuaSession::LuaSession(dom::Document& doc, std::string_view lua_source) {
    state_ = luaL_newstate();
    if (state_ == nullptr) {
        ok_ = false;
        error_ = "failed to allocate Lua state";
        return;
    }

    open_sandboxed_libs(state_);
    register_tv_api(state_, doc);
    lua_sethook(state_, count_hook, LUA_MASKCOUNT, kInstructionBudget);

    if (luaL_loadbuffer(state_, lua_source.data(), lua_source.size(), "script") != LUA_OK ||
        lua_pcall(state_, 0, 0, 0) != LUA_OK) {
        error_ = lua_tostring(state_, -1) != nullptr ? lua_tostring(state_, -1) : "load error";
        lua_pop(state_, 1);
        ok_ = false;
    }
}

LuaSession::~LuaSession() {
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

LuaSession::LuaSession(LuaSession&& other) noexcept
    : state_(other.state_), ok_(other.ok_), error_(std::move(other.error_)) {
    other.state_ = nullptr;
}

LuaSession& LuaSession::operator=(LuaSession&& other) noexcept {
    if (this != &other) {
        if (state_ != nullptr) {
            lua_close(state_);
        }
        state_ = other.state_;
        ok_ = other.ok_;
        error_ = std::move(other.error_);
        other.state_ = nullptr;
    }
    return *this;
}

ScriptOutcome LuaSession::call(std::string_view handler_name) {
    if (!ok_) {
        return {false, error_};
    }
    // Instruction budget is per-call, not cumulative across a session's
    // lifetime -- every click gets a fresh allowance.
    lua_sethook(state_, count_hook, LUA_MASKCOUNT, kInstructionBudget);

    const std::string name(handler_name);
    lua_getglobal(state_, name.c_str());
    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 1);
        return {false, "no such handler: " + name};
    }
    if (lua_pcall(state_, 0, 0, 0) != LUA_OK) {
        const char* msg = lua_tostring(state_, -1);
        ScriptOutcome out{false, msg != nullptr ? msg : "runtime error"};
        lua_pop(state_, 1);
        return out;
    }
    return {};
}

}  // namespace tvshow::script
