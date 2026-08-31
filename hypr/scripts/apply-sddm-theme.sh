#!/usr/bin/env bash
set -euo pipefail

# Link the dotfiles SDDM theme into /usr/share and select it.
# Uses ~/Downloads/bg.jpg at native resolution (no bundled copy).

THEME_NAME="where_is_my_sddm_theme"
THEME_SRC="${HOME}/.config/sddm/themes/${THEME_NAME}"
THEME_DST="/usr/share/sddm/themes/${THEME_NAME}"
CONF_DROPIN="/etc/sddm.conf.d/theme.d"
WALLPAPER="${HOME}/Downloads/bg.jpg"

if [[ ! -d "${THEME_SRC}" ]]; then
  echo "Missing theme directory: ${THEME_SRC}"
  exit 1
fi

if [[ ! -f "${WALLPAPER}" ]]; then
  echo "Wallpaper not found: ${WALLPAPER}"
  exit 1
fi

echo "Linking ${THEME_SRC} -> ${THEME_DST}"
sudo rm -rf "${THEME_DST}"
sudo ln -sfn "${THEME_SRC}" "${THEME_DST}"

printf '%s\n' "[Theme]" "Current=${THEME_NAME}" | sudo tee "${CONF_DROPIN}" >/dev/null

echo "SDDM theme applied: ${THEME_NAME}"
echo "  Wallpaper: ${WALLPAPER} (native resolution)"
echo ""
echo "Preview:  /usr/bin/sddm-greeter-qt6 --test-mode --theme ${THEME_DST}"
echo "          (Qt6 greeter — plain sddm-greeter needs Qt5 libs you don't have)"
echo "Apply:    log out, or  sudo systemctl restart sddm"
