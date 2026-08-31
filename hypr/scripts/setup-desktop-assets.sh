#!/usr/bin/env bash
set -euo pipefail

# Install desktop assets for the mint-vine rice (run once).
# Official Arch repos only + papirus-folders from GitHub (no AUR).
# Requires: sudo, network, git

PACMAN_PACKAGES=(
  papirus-icon-theme
  inter-font
  ttf-jetbrains-mono
  oxygen-cursors
  breeze
  qqc2-breeze-style
  breeze-gtk
)

install_pkg() {
  local pkg="$1"

  if pacman -Qi "$pkg" &>/dev/null; then
    echo "  ✓ $pkg (already installed)"
    return 0
  fi

  echo "  → $pkg"
  if sudo pacman -S --needed --noconfirm "$pkg"; then
    return 0
  fi

  echo "  ! mirror fetch failed for $pkg — refreshing databases..."
  sudo pacman -Syy
  if sudo pacman -S --needed --noconfirm "$pkg"; then
    return 0
  fi

  # Last resort: download package file directly, then install locally
  echo "  ! trying direct package download for $pkg..."
  if sudo pacman -Sw --noconfirm "$pkg" && sudo pacman -U --noconfirm /var/cache/pacman/pkg/"${pkg}"-*.pkg.tar.*; then
    return 0
  fi

  echo "  ✗ could not install $pkg"
  return 1
}

install_papirus_folders() {
  if command -v papirus-folders &>/dev/null; then
    return 0
  fi

  if ! command -v git &>/dev/null; then
    echo "Installing git (needed for papirus-folders)..."
    install_pkg git
  fi

  echo "Installing papirus-folders from GitHub (no AUR)..."
  local tmp
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  git clone --depth 1 https://github.com/PapirusDevelopmentTeam/papirus-folders.git "$tmp"
  sudo make -C "$tmp" install
}

FAILED=()

echo "Syncing package databases..."
sudo pacman -Sy

echo "Installing packages (one at a time so a single mirror miss does not block everything)..."
for pkg in "${PACMAN_PACKAGES[@]}"; do
  if ! install_pkg "$pkg"; then
    FAILED+=("$pkg")
  fi
done

if ((${#FAILED[@]})); then
  echo ""
  echo "Some packages could not be installed: ${FAILED[*]}"
  echo "Try manually:  sudo pacman -Syy && sudo pacman -S ${FAILED[*]}"
  echo "Continuing with what is available..."
  echo ""
fi

install_papirus_folders || FAILED+=("papirus-folders (git)")

if pacman -Qi papirus-icon-theme &>/dev/null && command -v papirus-folders &>/dev/null; then
  echo "Applying Papirus green folder accents (requires sudo)..."
  sudo papirus-folders -C green --theme Papirus-Dark
fi

echo "Refreshing font cache..."
fc-cache -fv >/dev/null

if pacman -Qi oxygen-cursors &>/dev/null; then
  echo "Applying Oxygen cursor in Hyprland session..."
  hyprctl setcursor Oxygen_White 24 2>/dev/null || true
  "${HOME}/.config/hypr/scripts/setup-cursor-compat.sh"
else
  echo "Oxygen cursors not installed — keeping current cursor theme."
fi

echo ""
if ((${#FAILED[@]})); then
  echo "Finished with warnings. Failed: ${FAILED[*]}"
else
  echo "Done."
fi
echo "  Icons:  Papirus-Dark (green folders)"
echo "  Cursor: Oxygen_White"
echo "  UI:     Inter 11"
echo "  Code:   JetBrains Mono"
echo ""
echo "Restart GTK/Qt apps (or log out) for icons/fonts to fully apply."

if ((${#FAILED[@]})); then
  exit 1
fi
