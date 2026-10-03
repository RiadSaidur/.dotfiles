#!/usr/bin/env bash
# Fix Android Emulator under Hyprland: grey GL + huge tiled white frame.
# Safe to re-run. Cold-boot the AVD afterward.
set -euo pipefail

ANDROID_HOME_DIR="${ANDROID_SDK_HOME:-$HOME/.android}"
mkdir -p "$ANDROID_HOME_DIR"

cat > "$ANDROID_HOME_DIR/advancedFeatures.ini" <<'EOF'
Vulkan = off
GLDirectMem = on
EOF
echo "wrote $ANDROID_HOME_DIR/advancedFeatures.ini"

shopt -s nullglob
for cfg in "$ANDROID_HOME_DIR"/avd/*.avd/config.ini; do
  avd_dir=$(dirname "$cfg")
  if grep -q '^hw.gpu.mode' "$cfg"; then
    sed -i 's/^hw.gpu.mode = .*/hw.gpu.mode = host/' "$cfg"
  else
    printf '\nhw.gpu.mode = host\n' >> "$cfg"
  fi
  cat > "$avd_dir/advancedFeatures.ini" <<'EOF'
Vulkan = off
GLDirectMem = on
EOF
  echo "updated $cfg"
done

echo
echo "Hyprland: emulator must FLOAT (tiling = giant empty white frame)."
echo "  Rules live in ~/.config/hypr/rules.lua — run: hyprctl reload"
echo
echo "In the running emulator also:"
echo "  ⋮ (more) → disable 'Show device frame' / frame around the device"
echo "  so the window shrink-wraps the phone surface."
echo
echo "Then cold-boot: Device Manager → ▾ on the AVD → Cold Boot Now"
echo
echo "If still broken:"
echo "  QT_QPA_PLATFORM=xcb emulator -avd <name> -gpu swiftshader_indirect"
echo
echo "Debug matchers:"
echo "  hyprctl clients | rg -i 'class|title|emulator|qemu'"
