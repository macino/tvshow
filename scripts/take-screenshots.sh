#!/usr/bin/env bash
# Take terminal screenshots of tvshow for docs.
# Requires: kitty, import (ImageMagick), xwininfo, DISPLAY set.
# Font: MxPlus IBM VGA 8x14 must be installed (~/.local/share/fonts/).

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

# ── start demo server ─────────────────────────────────────────────
"$SERVER" --port "$PORT" &
SRV_PID=$!
trap 'kill "$SRV_PID" 2>/dev/null; true' EXIT
sleep 1

# ── open a kitty window ───────────────────────────────────────────
# Usage: open_kitty SOCKET TITLE CMD [ARGS...]
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
    # wait up to 5s for socket to appear
    for _ in $(seq 1 25); do [[ -S "$sock" ]] && return; sleep 0.2; done
    echo "WARNING: socket $sock never appeared" >&2
}

# ── send a key to a kitty socket ─────────────────────────────────
kkey() { KITTY_LISTEN_ON="unix:$1" kitty @ send-key "${@:2}" 2>/dev/null || true; }
ktext() { KITTY_LISTEN_ON="unix:$1" kitty @ send-text "${@:2}" 2>/dev/null || true; }
kclose() { KITTY_LISTEN_ON="unix:$1" kitty @ close-window 2>/dev/null || true; }

# ── capture a window by title ─────────────────────────────────────
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

BASE="http://localhost:$PORT"

# ════════════════════════════════════════════════════════════════════
# startup  —  homepage
# ════════════════════════════════════════════════════════════════════
echo "startup.png"
S=/tmp/tvshow-ss1-$$.sock
open_kitty "$S" "tvshow-ss1" "$CLIENT" "$BASE/"
cap "tvshow-ss1" "$SHOTS/startup.png" 3
kclose "$S"
sleep 0.5

# ════════════════════════════════════════════════════════════════════
# address-bar  —  homepage + Ctrl-L
# ════════════════════════════════════════════════════════════════════
echo "address-bar.png"
S=/tmp/tvshow-ss2-$$.sock
open_kitty "$S" "tvshow-ss2" "$CLIENT" "$BASE/"
sleep 3
kkey "$S" ctrl+l
cap "tvshow-ss2" "$SHOTS/address-bar.png" 1
kclose "$S"
sleep 0.5

# ════════════════════════════════════════════════════════════════════
# forms  —  forms.html
# ════════════════════════════════════════════════════════════════════
echo "forms.png"
S=/tmp/tvshow-ss3-$$.sock
open_kitty "$S" "tvshow-ss3" "$CLIENT" "$BASE/pages/forms.html"
cap "tvshow-ss3" "$SHOTS/forms.png" 3
kclose "$S"
sleep 0.5

# ════════════════════════════════════════════════════════════════════
# typography  —  typography.html (also used for scroll demo)
# ════════════════════════════════════════════════════════════════════
echo "typography.png + scroll.png"
S=/tmp/tvshow-ss4-$$.sock
open_kitty "$S" "tvshow-ss4" "$CLIENT" "$BASE/pages/typography.html"
cap "tvshow-ss4" "$SHOTS/typography.png" 3
kkey "$S" down down down down down down down down
cap "tvshow-ss4" "$SHOTS/scroll.png" 1
kclose "$S"
sleep 0.5

# ════════════════════════════════════════════════════════════════════
# tabs  —  layout.html, then Ctrl-T + navigate to typography
# ════════════════════════════════════════════════════════════════════
echo "tabs.png"
S=/tmp/tvshow-ss5-$$.sock
open_kitty "$S" "tvshow-ss5" "$CLIENT" "$BASE/pages/layout.html"
sleep 3
kkey "$S" ctrl+t           # open new tab
sleep 1
kkey "$S" ctrl+l           # open address bar in new tab
sleep 0.5
ktext "$S" "$BASE/pages/typography.html"
kkey "$S" enter
cap "tvshow-ss5" "$SHOTS/tabs.png" 3
kclose "$S"
sleep 0.5

echo ""
echo "Done. Files in $SHOTS/:"
ls -lh "$SHOTS/"
