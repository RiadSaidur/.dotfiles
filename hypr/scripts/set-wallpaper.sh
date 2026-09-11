#!/usr/bin/env bash
# Set wallpaper and regenerate the desktop palette from it.
# Usage:
#   set-wallpaper.sh /path/to/image.jpg
#   set-wallpaper.sh                 # re-theme from current ~/Downloads/bg.jpg
set -euo pipefail

HOME="${HOME:-/home/syds}"
WALL_DST="${HOME}/Downloads/bg.jpg"
THEME_DIR="${HOME}/.config/theme"
SCRIPTS="${HOME}/.config/hypr/scripts"
PALETTE_PY="${SCRIPTS}/palette_from_wallpaper.py"
SDDM_BG="${HOME}/.config/sddm/themes/where_is_my_sddm_theme/background.jpg"

mkdir -p "${THEME_DIR}" "$(dirname "${WALL_DST}")"

SRC="${1:-}"
if [[ -n "${SRC}" ]]; then
  SRC="$(realpath -e "${SRC}")"
  if [[ "${SRC}" != "$(realpath -m "${WALL_DST}")" ]]; then
    echo "==> Installing wallpaper → ${WALL_DST}"
    cp -f "${SRC}" "${WALL_DST}"
  fi
elif [[ ! -f "${WALL_DST}" ]]; then
  echo "No wallpaper at ${WALL_DST}. Pass an image path." >&2
  exit 1
fi

chmod 644 "${WALL_DST}" 2>/dev/null || true

echo "==> Extracting palette (intensity matched to current theme)"
python3 "${PALETTE_PY}" apply "${WALL_DST}"

echo "==> Applying wallpaper to Hyprland"
if pgrep -x hyprpaper >/dev/null 2>&1; then
  hyprctl hyprpaper wallpaper ",${WALL_DST},cover" >/dev/null 2>&1 || true
  for mon in eDP-1 HDMI-A-2; do
    hyprctl hyprpaper wallpaper "${mon},${WALL_DST},cover" >/dev/null 2>&1 || true
  done
else
  hyprpaper &
  disown || true
fi

echo "==> GTK / Qt / KDE"
bash "${SCRIPTS}/apply-desktop-theme.sh" >/dev/null

echo "==> Firefox chrome colors"
bash "${SCRIPTS}/apply-firefox-theme.sh" >/dev/null || true

echo "==> SDDM wallpaper copy"
mkdir -p "$(dirname "${SDDM_BG}")"
cp -f "${WALL_DST}" "${SDDM_BG}"
chmod 644 "${SDDM_BG}" 2>/dev/null || true

echo "==> Reload Waybar / Mako"
if pgrep -x waybar >/dev/null 2>&1; then
  bash "${HOME}/.config/waybar/launch.sh" --reload >/dev/null 2>&1 || killall -SIGUSR2 waybar 2>/dev/null || true
fi
if pgrep -x mako >/dev/null 2>&1; then
  makoctl reload 2>/dev/null || { killall mako 2>/dev/null || true; mako & disown || true; }
fi
if pgrep -x kitty >/dev/null 2>&1; then
  killall -SIGUSR1 kitty 2>/dev/null || true
fi

if [[ -x "${SCRIPTS}/apply-chrome-theme.sh" ]]; then
  bash "${SCRIPTS}/apply-chrome-theme.sh" 2>/dev/null || echo "    (Chrome theme skipped — close Chrome and re-run apply-chrome-theme.sh)"
fi

printf '%s\n' "${WALL_DST}" >"${THEME_DIR}/wallpaper.path"

echo ""
echo "Done. Palette follows ${WALL_DST}"
echo "  Restart Firefox fully for chrome colors."
echo "  SDDM: log out to see the greeter wallpaper."
