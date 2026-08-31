#!/usr/bin/env bash
set -euo pipefail

# Single-instance Waybar launcher with cursor theme env for GTK hover cursors.

export XCURSOR_THEME="${XCURSOR_THEME:-Oxygen_White}"
export XCURSOR_SIZE="${XCURSOR_SIZE:-24}"
export XCURSOR_PATH="${HOME}/.local/share/icons:${HOME}/.icons:/usr/share/icons"

"${HOME}/.config/hypr/scripts/setup-cursor-compat.sh"

if [[ "${1:-}" == "--reload" ]]; then
    if pgrep -x waybar >/dev/null; then
        killall -SIGUSR2 waybar
        exit 0
    fi
fi

if pgrep -x waybar >/dev/null; then
    echo "waybar: already running (pid $(pgrep -x waybar | tr '\n' ' '))"
    echo "  reload:  ~/.config/waybar/launch.sh --reload"
    echo "  restart: killall waybar && ~/.config/waybar/launch.sh"
    exit 1
fi

exec waybar "$@"
