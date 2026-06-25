#!/usr/bin/env bash
# Take terminal screenshots of tvshow for docs and UI regression testing.
# Requires: kitty (with allow_remote_control=yes), ImageMagick import,
#           xwininfo, DISPLAY set, MxPlus IBM VGA 8x14 font installed.
#
# Usage:
#   ./scripts/take-screenshots.sh [BUILD_DIR]
#   BUILD_DIR defaults to ./build/release

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD=${1:-./build/release}
PORT=18499
FONT="MxPlus IBM VGA 8x14"
SHOTS=docs/screenshots

CLIENT="$BUILD/client/tvshow"
SERVER="$BUILD/server/tvshow-srv"

[[ -x "$CLIENT" ]] || { echo "client not found: $CLIENT"; exit 1; }
[[ -x "$SERVER" ]] || { echo "server not found: $SERVER"; exit 1; }

mkdir -p "$SHOTS"

BASE="http://localhost:$PORT"

# ── start demo server ─────────────────────────────────────────────
"$SERVER" --port "$PORT" &
SRV_PID=$!
trap 'kill "$SRV_PID" 2>/dev/null; true' EXIT
sleep 1

# ── helpers ───────────────────────────────────────────────────────

open_kitty() {
    local sock="$1" title="$2"; shift 2
    rm -f "$sock"
    DISPLAY=:0 kitty \
        --listen-on "unix:$sock" \
        --override "allow_remote_control=yes" \
        --override "font_family=$FONT" \
        --override "font_size=14.0" \
        --override "initial_window_columns=80" \
        --override "initial_window_lines=25" \
        --override "remember_window_size=no" \
        --override "window_padding_width=0" \
        --override "tab_bar_style=hidden" \
        --title "$title" \
        "$@" &
    for _ in $(seq 1 25); do [[ -S "$sock" ]] && return; sleep 0.2; done
    echo "WARNING: socket $sock never appeared" >&2
}

kkey()  { KITTY_LISTEN_ON="unix:$1" kitty @ send-key  "${@:2}" 2>/dev/null || true; }
ktext() { KITTY_LISTEN_ON="unix:$1" kitty @ send-text "${@:2}" 2>/dev/null || true; }
kclose(){ KITTY_LISTEN_ON="unix:$1" kitty @ close-window 2>/dev/null || true; }

cap() {
    local title="$1" file="$2" delay="${3:-2}"
    sleep "$delay"
    local wid
    wid=$(DISPLAY=:0 xwininfo -name "$title" 2>/dev/null | awk '/Window id:/{print $4}') || true
    if [[ -n "$wid" ]]; then
        DISPLAY=:0 import -window "$wid" "$file"
        echo "  ok  $file"
    else
        echo "  FAIL  window '$title' not found" >&2
    fi
}

shot() {
    local label="$1" url="$2" file="$3"
    local sock="/tmp/tvshow-ss-$$-${label}.sock"
    local title="tvshow-ss-${label}"
    echo "$file"
    open_kitty "$sock" "$title" "$CLIENT" "$url"
    cap "$title" "$file" 3
    kclose "$sock"; sleep 0.5
}

# ════════════════════════════════════════════════════════════════════
# DOCS screenshots (README + user-guide)
# ════════════════════════════════════════════════════════════════════

echo "=== docs ==="

# startup — homepage
shot "startup" "$BASE/" "$SHOTS/startup.png"

# address-bar — homepage + Ctrl-L
echo "$SHOTS/address-bar.png"
S=/tmp/tvshow-ss-addrbar-$$.sock
open_kitty "$S" "tvshow-addrbar" "$CLIENT" "$BASE/"
sleep 3; kkey "$S" ctrl+l
cap "tvshow-addrbar" "$SHOTS/address-bar.png" 1
kclose "$S"; sleep 0.5

# forms — demo forms page
shot "forms" "$BASE/pages/forms.html" "$SHOTS/forms.png"

# typography — typography page, then scroll
echo "$SHOTS/typography.png + $SHOTS/scroll.png"
S=/tmp/tvshow-ss-typo-$$.sock
open_kitty "$S" "tvshow-typo" "$CLIENT" "$BASE/pages/typography.html"
cap "tvshow-typo" "$SHOTS/typography.png" 3
kkey "$S" down down down down down down down down
cap "tvshow-typo" "$SHOTS/scroll.png" 1
kclose "$S"; sleep 0.5

# tabs — layout page, Ctrl-T, navigate to typography
echo "$SHOTS/tabs.png"
S=/tmp/tvshow-ss-tabs-$$.sock
open_kitty "$S" "tvshow-tabs" "$CLIENT" "$BASE/pages/layout.html"
sleep 3
kkey "$S" ctrl+t; sleep 1
kkey "$S" ctrl+l; sleep 0.5
ktext "$S" "$BASE/pages/typography.html"; kkey "$S" enter
cap "tvshow-tabs" "$SHOTS/tabs.png" 3
kclose "$S"; sleep 0.5

# ════════════════════════════════════════════════════════════════════
# M1 feature screenshots
# ════════════════════════════════════════════════════════════════════

echo ""
echo "=== M1 features ==="

# tables — column alignment demo
shot "tables" "$BASE/pages/tables.html" "$SHOTS/tables.png"

# position — relative/absolute positioning
shot "position" "$BASE/pages/position.html" "$SHOTS/position.png"

# hover — static shot (hover needs mouse interaction, capture at rest)
shot "hover" "$BASE/pages/hover.html" "$SHOTS/hover.png"

# themes — all three variants
shot "theme-tv"    "$BASE/pages/themes.html"       "$SHOTS/theme-tvision.png"
shot "theme-dark"  "$BASE/pages/themes-dark.html"  "$SHOTS/theme-dark.png"
shot "theme-light" "$BASE/pages/themes-light.html" "$SHOTS/theme-light.png"

# debug overlay — Ctrl-D on elements page
echo "$SHOTS/debug-overlay.png"
S=/tmp/tvshow-ss-dbgovl-$$.sock
open_kitty "$S" "tvshow-dbgovl" "$CLIENT" "$BASE/pages/debug/elements.html"
sleep 3; kkey "$S" ctrl+d
cap "tvshow-dbgovl" "$SHOTS/debug-overlay.png" 1
kclose "$S"; sleep 0.5

# ════════════════════════════════════════════════════════════════════
# BUG screenshots — inline control overlap
# ════════════════════════════════════════════════════════════════════

mkdir -p "$SHOTS/debug"

echo ""
echo "=== bug: inline-controls ==="
shot "f1" "$BASE/pages/debug/inline-controls.html" "$SHOTS/debug/inline-controls.png"

# ════════════════════════════════════════════════════════════════════
# UI regression screenshots — one per debug fixture
# ════════════════════════════════════════════════════════════════════

echo ""
echo "=== ui regression ==="
shot "elements"   "$BASE/pages/debug/elements.html"   "$SHOTS/debug/elements.png"
shot "css-props"  "$BASE/pages/debug/css-props.html"  "$SHOTS/debug/css-props.png"
shot "flex"       "$BASE/pages/debug/flex.html"        "$SHOTS/debug/flex.png"
shot "forms-full" "$BASE/pages/debug/forms-full.html" "$SHOTS/debug/forms-full.png"
shot "links"      "$BASE/pages/debug/links.html"       "$SHOTS/debug/links.png"

# long page — top
shot "long-top"   "$BASE/pages/debug/long.html"        "$SHOTS/debug/long-top.png"

# long page — scrolled to section 10
echo "$SHOTS/debug/long-scroll.png"
S=/tmp/tvshow-ss-longscr-$$.sock
open_kitty "$S" "tvshow-longscr" "$CLIENT" "$BASE/pages/debug/long.html"
sleep 3
for _ in $(seq 1 15); do kkey "$S" down; sleep 0.05; done
cap "tvshow-longscr" "$SHOTS/debug/long-scroll.png" 1
kclose "$S"; sleep 0.5

# long page — fragment jump to #s10
shot "long-frag" "$BASE/pages/debug/long.html#s10" "$SHOTS/debug/long-fragment.png"

# ════════════════════════════════════════════════════════════════════
# tabs cascade — T1/T2/T3
# ════════════════════════════════════════════════════════════════════

echo ""
echo "=== tabs cascade ==="

echo "$SHOTS/debug/tabs-t1.png (1 window)"
S=/tmp/tvshow-ss-t1-$$.sock
open_kitty "$S" "tvshow-t1" "$CLIENT" "$BASE/pages/debug/elements.html"
cap "tvshow-t1" "$SHOTS/debug/tabs-t1.png" 3
kclose "$S"; sleep 0.5

echo "$SHOTS/debug/tabs-t2.png (2 windows)"
S=/tmp/tvshow-ss-t2-$$.sock
open_kitty "$S" "tvshow-t2" "$CLIENT" "$BASE/pages/debug/elements.html"
sleep 3
kkey "$S" ctrl+t; sleep 1
kkey "$S" ctrl+l; sleep 0.5
ktext "$S" "$BASE/pages/debug/css-props.html"; kkey "$S" enter
cap "tvshow-t2" "$SHOTS/debug/tabs-t2.png" 3
kclose "$S"; sleep 0.5

echo "$SHOTS/debug/tabs-t3.png (3 windows)"
S=/tmp/tvshow-ss-t3-$$.sock
open_kitty "$S" "tvshow-t3" "$CLIENT" "$BASE/pages/debug/elements.html"
sleep 3
kkey "$S" ctrl+t; sleep 1
kkey "$S" ctrl+l; sleep 0.5; ktext "$S" "$BASE/pages/debug/css-props.html"; kkey "$S" enter
sleep 2
kkey "$S" ctrl+t; sleep 1
kkey "$S" ctrl+l; sleep 0.5; ktext "$S" "$BASE/pages/debug/flex.html"; kkey "$S" enter
cap "tvshow-t3" "$SHOTS/debug/tabs-t3.png" 4
kclose "$S"; sleep 0.5

echo ""
echo "Done."
ls -lh "$SHOTS/"
ls -lh "$SHOTS/debug/" 2>/dev/null || true
