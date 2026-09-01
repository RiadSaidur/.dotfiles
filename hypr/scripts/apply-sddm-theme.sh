#!/usr/bin/env bash
set -euo pipefail

# Install SDDM theme + copy wallpaper into the theme (sddm user cannot read ~/Downloads).
# Source wallpaper: ~/Downloads/bg.jpg (same as hyprpaper / hyprlock).

THEME_NAME="where_is_my_sddm_theme"
THEME_SRC="${HOME}/.config/sddm/themes/${THEME_NAME}"
THEME_DST="/usr/share/sddm/themes/${THEME_NAME}"
CONF_DROPIN="/etc/sddm.conf.d/theme.d"
WALLPAPER="${HOME}/Downloads/bg.jpg"
THEME_BG="${THEME_SRC}/background.jpg"

if [[ ! -d "${THEME_SRC}" ]]; then
  echo "Missing theme directory: ${THEME_SRC}"
  exit 1
fi

if [[ ! -f "${WALLPAPER}" ]]; then
  echo "Wallpaper not found: ${WALLPAPER}"
  exit 1
fi

echo "Installing wallpaper for SDDM greeter (world-readable copy in theme)..."
cp -f "${WALLPAPER}" "${THEME_BG}"
chmod 644 "${THEME_BG}"

# sddm (uid sddm) must traverse $HOME and .config — warn if blocked
for dir in "${HOME}" "${HOME}/.config" "${HOME}/.config/sddm" "${THEME_SRC}"; do
  if [[ ! -r "${dir}" || ! -x "${dir}" ]]; then
    echo "WARN: ${dir} is not world-executable; SDDM may fail to load the theme."
    echo "      Fix: chmod o+x ${dir}"
  fi
done

if ! sudo -u sddm test -r "${THEME_BG}" 2>/dev/null; then
  echo "WARN: user sddm still cannot read ${THEME_BG}"
  echo "      Ensure parent dirs under ${HOME} allow traverse (chmod o+x)."
else
  echo "OK: sddm can read ${THEME_BG}"
fi

echo "Linking ${THEME_SRC} -> ${THEME_DST}"
sudo rm -rf "${THEME_DST}"
sudo ln -sfn "${THEME_SRC}" "${THEME_DST}"

printf '%s\n' "[Theme]" "Current=${THEME_NAME}" | sudo tee "${CONF_DROPIN}" >/dev/null

echo ""
echo "SDDM theme applied: ${THEME_NAME}"
echo "  Wallpaper: ${THEME_BG}"
echo ""
echo "Preview:  /usr/bin/sddm-greeter-qt6 --test-mode --theme ${THEME_DST}"
echo "Apply:    log out, or  sudo systemctl restart sddm"
echo "Re-run this script after changing ~/Downloads/bg.jpg"
