#pragma once

#include "tvshow/dom/node.hpp"

#include <string>
#include <string_view>

struct lua_State;

namespace tvshow::script {

struct ScriptOutcome {
    bool ok = true;
    std::string error;  // set only when ok == false
};

// adr-sandboxed-scripting: one sandboxed Lua VM per loaded page (not per
// click) -- a script's top-level `local` variables (a running calculator
// total, say) are upvalues closed over by its `function` globals, and
// closures only keep working across calls if the VM that created them is
// still alive. A fresh VM per click would silently reset that state on
// every keypress; this type exists specifically so callers don't do that.
//
// Sandboxing (structural, not just policy -- see lua_engine.cpp):
//   - only base/string/math/table libs opened -- io/os/package/debug never
//     linked in, so filesystem/process/env access doesn't exist to reach
//   - load/loadstring/dofile/loadfile/require stripped from base after
//     opening it -- a script can't load new code strings at runtime
//   - an instruction-count hook aborts any single call past a fixed budget,
//     bounding runaway/infinite loops deterministically (no wall-clock read,
//     stays a pure-function contract per SPEC Sec 3.1)
//
// Mutation surface: `tv.set_text(id, str)` / `tv.get_text(id)`, scoped to
// existing elements' text content by `id` attribute. No node insertion or
// removal -- every element a script touches must already exist in the
// page's initial HTML with its placeholder text.
class LuaSession {
public:
    // Creates a sandboxed VM bound to `doc` (which must outlive this
    // session) and runs `lua_source` once, defining its top-level functions
    // and initializing its locals. On failure, ok() is false and error()
    // explains why; call() is then a no-op returning the same failure.
    LuaSession(dom::Document& doc, std::string_view lua_source);
    ~LuaSession();

    LuaSession(const LuaSession&) = delete;
    LuaSession& operator=(const LuaSession&) = delete;
    LuaSession(LuaSession&&) noexcept;
    LuaSession& operator=(LuaSession&&) noexcept;

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    // Calls the zero-argument global function named `handler_name`,
    // mutating the bound Document in place via tv.set_text. Returns
    // ok=false (doc left as whatever the script managed before failing --
    // no rollback) if the function doesn't exist or errors.
    [[nodiscard]] ScriptOutcome call(std::string_view handler_name);

private:
    ::lua_State* state_ = nullptr;
    bool ok_ = true;
    std::string error_;
};

}  // namespace tvshow::script
