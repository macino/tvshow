#!/usr/bin/env python3
"""adr-extension-server + adr-sandboxed-scripting: the same calculator,
re-hosted as a single static page whose keypad runs entirely client-side via
a sandboxed Lua VM tvshow embeds -- no per-click round trip through this
script at all. Earlier versions of this file were an HTML-forms /
links-with-a-round-trip design; both were replaced once client-side
scripting landed, because every click was a full page reload ("blinking")
and neither one could look/behave like the native Tools -> Calculator
window. See adr-extension-server.md's "round two" section for the two real
tvshow behaviors (not guesses) that ruled out plain forms/links for a
button grid, and adr-sandboxed-scripting.md for the Lua sandboxing details.

Speaks the same CGI-lite protocol as before for the *initial* GET (see
docs/decisions/adr-extension-server.md) -- this script only runs once, to
serve the page. Every button press after that runs inside tvshow's process,
not this one.

Design note: the Lua side is a running-total accumulator (like a physical
4-function calculator), not an expression-string parser -- deliberately
sidesteps needing any eval()-equivalent, consistent with tvshow's sandboxed
Lua stripping `load`/`loadstring` entirely. No parentheses key as a result
(an accumulator has no notion of operator precedence to group) -- a real,
different-but-valid calculator design, not a cut corner of the AST-walking
one calculator.py/calc_window.cpp use.

Unrelated to calculator.py (the window-provider structured-UI reference
script next to this file) -- see extensions/README.md for how the
mechanisms differ.
"""

import html
import sys

_KEYS = [
    ("C", "kC"), ("<", "kBack"), ("/", "kDiv"), ("*", "kMul"),
    ("7", "k7"), ("8", "k8"), ("9", "k9"), ("-", "kSub"),
    ("4", "k4"), ("5", "k5"), ("6", "k6"), ("+", "kAdd"),
    ("1", "k1"), ("2", "k2"), ("3", "k3"), ("=", "kEq"),
    ("0", "k0"), (".", "kDot"),
]

_LUA_SOURCE = """
local entry = "0"
local total = 0
local pending_op = nil

local function fmt(n)
  if n ~= n then return "error" end             -- NaN
  if n == math.floor(n) and math.abs(n) < 1e15 then
    return tostring(math.floor(n))
  end
  return tostring(n)
end

local function refresh()
  tv.set_text("display", entry)
end

local function digit(d)
  if entry == "0" then entry = d else entry = entry .. d end
  refresh()
end

local function apply_pending()
  local n = tonumber(entry) or 0
  if pending_op == nil then
    total = n
  elseif pending_op == "+" then
    total = total + n
  elseif pending_op == "-" then
    total = total - n
  elseif pending_op == "*" then
    total = total * n
  elseif pending_op == "/" then
    total = (n ~= 0) and (total / n) or (0 / 0)
  end
end

local function op(o)
  apply_pending()
  pending_op = o
  entry = "0"
  tv.set_text("display", fmt(total))
end

function kC()
  entry, total, pending_op = "0", 0, nil
  refresh()
end

function kBack()
  if #entry > 1 then entry = entry:sub(1, #entry - 1) else entry = "0" end
  refresh()
end

function kDot()
  if not entry:find(".", 1, true) then entry = entry .. "." end
  refresh()
end

function kAdd() op("+") end
function kSub() op("-") end
function kMul() op("*") end
function kDiv() op("/") end

function kEq()
  apply_pending()
  pending_op = nil
  entry = fmt(total)
  refresh()
end

function k0() digit("0") end
function k1() digit("1") end
function k2() digit("2") end
function k3() digit("3") end
function k4() digit("4") end
function k5() digit("5") end
function k6() digit("6") end
function k7() digit("7") end
function k8() digit("8") end
function k9() digit("9") end
"""


def render() -> str:
    buttons = " ".join(
        f'<button onclick="{fn}" value="{html.escape(label)}">{html.escape(label)}</button>'
        for label, fn in _KEYS
    )
    return f"""<!doctype html>
<html>
<head>
<title>Calculator</title>
<meta name="tvshow-window-size" content="26x14">
<meta name="tvshow-window-color" content="gray">
<style>
/* tvshow-window-color only recolors the window frame/chrome (a TWindow
   getPalette() override) -- the content area is CharGrid the render
   pipeline paints from CSS, entirely separate from TView's palette system.
   Matching the native gray Calculator's fill needs standard CSS, not a
   tvshow-specific hint -- this is that. */
body {{ background: silver; color: black; }}
/* #display must be a block element (a <div>, not <span>) -- this render
   pipeline only paints per-element background on block-level/leaf-control
   boxes; a plain inline <span>'s background-color is silently a no-op
   (confirmed live: swap this to <span> and the blue never shows). */
#display {{ background: blue; color: white; }}
button {{ background: green; color: black; }}
</style>
</head>
<body>
<h1>Calculator</h1>
<div id="display">0</div>
<p>{buttons}</p>
<script type="text/lua">
{_LUA_SOURCE}
</script>
</body>
</html>
"""


def main() -> None:
    sys.stdin.read()  # drain the request; nothing in it changes this response
    page = render()
    sys.stdout.write("Status: 200\n")
    sys.stdout.write("Content-Type: text/html\n")
    sys.stdout.write("\n")
    sys.stdout.write(page)
    sys.stdout.flush()


if __name__ == "__main__":
    main()
