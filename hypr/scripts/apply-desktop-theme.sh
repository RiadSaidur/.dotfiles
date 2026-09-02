#!/usr/bin/env bash
# Apply unified mint-vine GTK + Qt theming for Hyprland.
# GTK4/libadwaita (Nautilus): mint-vine colors via ~/.config/gtk-4.0/gtk.css (no Catppuccin greys).
# GTK3: prefer Breeze-Dark when breeze-gtk is installed (matches KDE).
set -euo pipefail

ICON_THEME="Papirus-Dark"
CURSOR_THEME="Oxygen_White"
CURSOR_SIZE=24
COLOR_SCHEME_SRC="${HOME}/.config/kde-color-schemes/MintVine.colors"
COLOR_SCHEME_DST="${HOME}/.local/share/color-schemes/MintVine.colors"
GTK4_MINT_CSS="${HOME}/.config/gtk-4.0/gtk-mint-vine.css"

if [[ -d /usr/share/themes/Breeze-Dark ]]; then
  GTK_THEME="Breeze-Dark"
elif [[ -d /usr/share/themes/Breeze ]]; then
  GTK_THEME="Breeze"
else
  GTK_THEME="Adwaita"
fi

echo "==> Packages (official repos)"
echo "    GTK:      breeze-gtk (optional, for Breeze-Dark)  papirus-icon-theme  oxygen-cursors"
echo "    Qt/KDE:   qt6ct  breeze  qqc2-breeze-style"
echo ""

mkdir -p "${HOME}/.local/share/color-schemes" \
         "${HOME}/.config/gtk-3.0" \
         "${HOME}/.config/gtk-4.0"

if [[ -f "${COLOR_SCHEME_SRC}" ]]; then
  cp -f "${COLOR_SCHEME_SRC}" "${COLOR_SCHEME_DST}" 2>/dev/null || true
fi

# GTK 3/4 settings.ini
for ini in "${HOME}/.config/gtk-3.0/settings.ini" "${HOME}/.config/gtk-4.0/settings.ini"; do
  cat >"${ini}" <<EOF
[Settings]
gtk-theme-name=${GTK_THEME}
gtk-icon-theme-name=${ICON_THEME}
gtk-font-name=Inter 11
gtk-cursor-theme-name=${CURSOR_THEME}
gtk-cursor-theme-size=${CURSOR_SIZE}
gtk-application-prefer-dark-theme=1
EOF
done

# Extra GTK3 keys
cat >"${HOME}/.config/gtk-3.0/settings.ini" <<EOF
[Settings]
gtk-theme-name=${GTK_THEME}
gtk-icon-theme-name=${ICON_THEME}
gtk-font-name=Inter 11
gtk-cursor-theme-name=${CURSOR_THEME}
gtk-cursor-theme-size=${CURSOR_SIZE}
gtk-toolbar-style=GTK_TOOLBAR_ICONS
gtk-toolbar-icon-size=GTK_ICON_SIZE_LARGE_TOOLBAR
gtk-button-images=1
gtk-menu-images=1
gtk-enable-event-sounds=1
gtk-enable-input-feedback-sounds=0
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintslight
gtk-xft-rgba=rgb
gtk-application-prefer-dark-theme=1
EOF

# gsettings (libadwaita / many GTK4 apps read this — was stuck on prefer-light)
if command -v gsettings >/dev/null; then
  gsettings set org.gnome.desktop.interface gtk-theme "${GTK_THEME}"
  gsettings set org.gnome.desktop.interface icon-theme "${ICON_THEME}"
  gsettings set org.gnome.desktop.interface cursor-theme "${CURSOR_THEME}"
  gsettings set org.gnome.desktop.interface cursor-size "${CURSOR_SIZE}"
  gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
  gsettings set org.gnome.desktop.interface accent-color 'green'
  gsettings set org.gnome.desktop.interface font-name 'Inter 11'
fi

# KDE/Kirigami: ColorScheme=name alone is NOT enough outside Plasma.
# System Settings would copy Colors:* into kdeglobals — we do that here.
mkdir -p "${HOME}/.local/share/color-schemes" "${HOME}/.config/kde-color-schemes"
if [[ -f "${COLOR_SCHEME_SRC}" ]]; then
  cp -f "${COLOR_SCHEME_SRC}" "${COLOR_SCHEME_DST}"
fi

KDEGLOBALS="${HOME}/.config/kdeglobals"
WIDGET_STYLE="Fusion"
pacman -Qi breeze &>/dev/null && WIDGET_STYLE="Breeze"

{
  cat <<EOF
[General]
ColorScheme=MintVine
TerminalApplication=kitty
TerminalService=kitty.desktop

[Icons]
Theme=${ICON_THEME}

[KDE]
widgetStyle=${WIDGET_STYLE}
contrast=4

[UiSettings]
ColorScheme=MintVine

EOF
  # Append color groups from the scheme file (skip its [General] name block / duplicate headers)
  if [[ -f "${COLOR_SCHEME_SRC}" ]]; then
    awk '
      /^\[General\]/ { skip=1; next }
      /^\[KDE\]/ { skip=1; next }
      /^\[/ { skip=0 }
      skip==0 { print }
    ' "${COLOR_SCHEME_SRC}"
  fi
} >"${KDEGLOBALS}"

if pacman -Qi qqc2-breeze-style &>/dev/null; then
  QQC_STYLE="org.kde.breeze"
else
  QQC_STYLE="org.kde.desktop"
  echo "    WARN: qqc2-breeze-style missing — KDE Connect stays light until installed"
fi
ENV_D="${HOME}/.config/environment.d/99-mint-vine-desktop.conf"
mkdir -p "${HOME}/.config/environment.d"
cat >"${ENV_D}" <<EOF
# Session env for GTK/Qt/KDE apps (systemd user + dbus activation).
GTK_THEME=${GTK_THEME}
QT_QPA_PLATFORMTHEME=qt6ct
QT_QUICK_CONTROLS_STYLE=${QQC_STYLE}
EOF

# Icon cache (Nautilus context-menu icons inherit breeze-dark from Papirus)
if command -v gtk-update-icon-cache &>/dev/null; then
  for theme in Papirus-Dark breeze-dark hicolor; do
    [[ -d "/usr/share/icons/${theme}" ]] && gtk-update-icon-cache -f "/usr/share/icons/${theme}" 2>/dev/null || true
  done
fi


# GTK4/libadwaita: mint-vine palette only (Catppuccin greys clash with KDE MintVine)
GTK4_DIR="${HOME}/.config/gtk-4.0"
if [[ -f "${GTK4_MINT_CSS}" ]]; then
  cp -f "${GTK4_MINT_CSS}" "${GTK4_DIR}/gtk.css"
else
  echo "    WARN: missing ${GTK4_MINT_CSS}"
fi
# Drop Catppuccin gtk-4 symlinks if present
rm -f "${GTK4_DIR}/gtk-dark.css" "${GTK4_DIR}/assets"

# qt6ct: Fusion + mint-vine palette
QT6CT="${HOME}/.config/qt6ct/qt6ct.conf"
if [[ -f "${QT6CT}" ]]; then
  sed -i 's|^color_scheme_path=.*|color_scheme_path='"${HOME}"'/.config/qt6ct/colors/mint-vine.conf|' "${QT6CT}"
  sed -i 's|^custom_palette=.*|custom_palette=true|' "${QT6CT}"
  sed -i 's|^icon_theme=.*|icon_theme=Papirus-Dark|' "${QT6CT}"
  if pacman -Qi breeze &>/dev/null; then
    sed -i 's|^style=.*|style=Breeze|' "${QT6CT}"
  else
    sed -i 's|^style=.*|style=Fusion|' "${QT6CT}"
  fi
fi

# LibreOffice: dark chrome + white document/cells (no-op if soffice is running)
if [[ -x "${HOME}/.config/hypr/scripts/apply-libreoffice-theme.sh" ]]; then
  "${HOME}/.config/hypr/scripts/apply-libreoffice-theme.sh" 2>/dev/null || true
fi

echo "==> Applied"
echo "    GTK:    ${GTK_THEME} + mint-vine libadwaita CSS"
echo "    Icons:  ${ICON_THEME}"
echo "    Cursor: ${CURSOR_THEME} ${CURSOR_SIZE}"
echo "    Qt:     qt6ct + mint-vine"
echo "    KDE:    ColorScheme=MintVine, QQC=${QQC_STYLE}"
echo ""
if ! pacman -Qi breeze-gtk &>/dev/null; then
  echo "Optional (GTK3 Breeze match):  sudo pacman -S --needed breeze-gtk"
fi
if ! pacman -Qi qqc2-breeze-style &>/dev/null || ! pacman -Qi breeze &>/dev/null; then
  echo "Install KDE styling:  sudo pacman -S --needed breeze qqc2-breeze-style"
fi
echo "Restart Nautilus (or log out) for full effect."
