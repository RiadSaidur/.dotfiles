#!/usr/bin/env bash
# Oxygen_White is missing GTK cursor names (arrow, default). A partial user theme
# dir shadows the system copy, so we ship the handful of names GTK asks for.

set -euo pipefail

THEME_NAME="Oxygen_White"
SYSTEM_CURSORS="/usr/share/icons/${THEME_NAME}/cursors"
USER_THEME="${HOME}/.local/share/icons/${THEME_NAME}"
USER_CURSORS="${USER_THEME}/cursors"

if [[ ! -d "${SYSTEM_CURSORS}" ]]; then
    exit 0
fi

mkdir -p "${USER_CURSORS}"

copy_cursor() {
    local name="$1"
    local src="${SYSTEM_CURSORS}/${name}"

    if [[ -L "${src}" ]]; then
        src="${SYSTEM_CURSORS}/$(readlink "${src}")"
    fi

    if [[ -f "${src}" ]]; then
        cp -f "${src}" "${USER_CURSORS}/${name}"
    fi
}

# GTK / Waybar hover cursors
for name in left_ptr pointing_hand hand2; do
    copy_cursor "${name}"
done

# GTK expects these X11 names; Oxygen only has left_ptr
if [[ -f "${USER_CURSORS}/left_ptr" ]]; then
    cp -f "${USER_CURSORS}/left_ptr" "${USER_CURSORS}/arrow"
    cp -f "${USER_CURSORS}/left_ptr" "${USER_CURSORS}/default"
fi

# Drop inherited index.theme — it made GTK treat this as a partial theme override
rm -f "${USER_THEME}/index.theme"
