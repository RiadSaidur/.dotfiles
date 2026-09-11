#!/usr/bin/env bash
# Launch (or restart) the shadow-cat desktop pet.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$DIR/shadow-cat"
SRC="$DIR/shadow_cat.c"

# Always rebuild when missing or source is newer than the binary.
if [[ ! -x "$BIN" || "$SRC" -nt "$BIN" ]]; then
  make -C "$DIR" -s
fi

pkill -x shadow-cat 2>/dev/null || true
sleep 0.08

# Detach from Hyprland's exec helper so the pet outlives the launcher.
# Do not forward "$@" — avoids passing untrusted argv into GTK.
exec setsid -f "$BIN"
