#!/usr/bin/env bash
# LibreOffice mint-vine chrome: dark UI toolbars/menus, white Calc cells untouched.
# Safe to re-run. Close LibreOffice first so registry isn't overwritten on exit.
set -euo pipefail

USER_HOME="${HOME}"
LO_USER="${USER_HOME}/.config/libreoffice/4/user"
REG="${LO_USER}/registrymodifications.xcu"

if pgrep -x soffice.bin >/dev/null 2>&1; then
  echo "Close LibreOffice fully first (prefs get overwritten while running)."
  echo "  pkill -x soffice.bin; then re-run this script."
  exit 1
fi

mkdir -p "${LO_USER}"

# Fix accidental root ownership (common after sandbox/root launches)
if [[ -d "${USER_HOME}/.config/libreoffice" ]]; then
  owner="$(stat -c '%u' "${USER_HOME}/.config/libreoffice" 2>/dev/null || echo)"
  me="$(id -u)"
  if [[ -n "${owner}" && "${owner}" != "${me}" ]]; then
    echo "==> Fixing libreoffice config ownership (${owner} → ${me})"
    chown -R "${me}:${me}" "${USER_HOME}/.config/libreoffice" 2>/dev/null || \
      sudo chown -R "${me}:${me}" "${USER_HOME}/.config/libreoffice"
  fi
fi

python3 - "${REG}" <<'PY'
import sys
from pathlib import Path

reg = Path(sys.argv[1])

# Minimal registry if missing
if not reg.is_file():
    reg.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<oor:items xmlns:oor="http://openoffice.org/2001/registry" '
        'xmlns:xs="http://www.w3.org/2001/XMLSchema" '
        'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">\n'
        '</oor:items>\n',
        encoding="utf-8",
    )

text = reg.read_text(encoding="utf-8")

def upsert(path: str, name: str, value: str, value_type: str = "plain") -> None:
    """Insert or replace a fuse prop under oor:path."""
    global text
    import re
    # Match the whole <item ...>...</item> for this path+name
    pat = re.compile(
        rf'<item oor:path="{re.escape(path)}">\s*'
        rf'<prop oor:name="{re.escape(name)}"[^>]*>.*?</prop>\s*</item>',
        re.DOTALL,
    )
    if value_type == "bool":
        inner = f'<prop oor:name="{name}" oor:op="fuse"><value>{value}</value></prop>'
    else:
        inner = f'<prop oor:name="{name}" oor:op="fuse"><value>{value}</value></prop>'
    item = f'<item oor:path="{path}">{inner}</item>'
    if pat.search(text):
        text = pat.sub(item, text, count=1)
    else:
        # insert before closing </oor:items>
        if "</oor:items>" not in text:
            raise SystemExit("invalid registrymodifications.xcu")
        text = text.replace("</oor:items>", f"{item}\n</oor:items>", 1)

# 0=Automatic, 1=Light, 2=Dark — dark chrome
upsert("/org.openoffice.Office.Common/Appearance", "ApplicationAppearance", "2")
# Keep spreadsheet / document backgrounds white regardless of dark UI
upsert("/org.openoffice.Office.Common/Appearance", "UseOnlyWhiteDocBackground", "true", "bool")
# Light application color scheme = light document/grid colors
upsert(
    "/org.openoffice.Office.UI/ColorScheme",
    "CurrentColorScheme",
    "COLOR_SCHEME_LIBREOFFICE_LIGHT",
)

reg.write_text(text, encoding="utf-8")
print(f"==> Patched {reg}")
print("    ApplicationAppearance = Dark (2)")
print("    UseOnlyWhiteDocBackground = true  (cells stay white)")
print("    CurrentColorScheme = COLOR_SCHEME_LIBREOFFICE_LIGHT")
PY

# Prefer KDE/Qt plugin so MintVine kdeglobals palette paints LO chrome
ENV_D="${USER_HOME}/.config/environment.d/99-mint-vine-desktop.conf"
mkdir -p "${USER_HOME}/.config/environment.d"
if [[ -f "${ENV_D}" ]]; then
  if ! grep -q '^SAL_USE_VCLPLUGIN=' "${ENV_D}" 2>/dev/null; then
    printf '\n# LibreOffice: use KDE colors (MintVine) for chrome; cells stay white via LO prefs\nSAL_USE_VCLPLUGIN=kf6\n' >>"${ENV_D}"
  else
    sed -i 's|^SAL_USE_VCLPLUGIN=.*|SAL_USE_VCLPLUGIN=kf6|' "${ENV_D}"
  fi
else
  cat >"${ENV_D}" <<'EOF'
SAL_USE_VCLPLUGIN=kf6
EOF
fi

# Session env for current Hyprland
if command -v hyprctl >/dev/null && [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" || -d /run/user/$(id -u)/hypr ]]; then
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
  export HYPRLAND_INSTANCE_SIGNATURE="${HYPRLAND_INSTANCE_SIGNATURE:-$(ls -1 /run/user/$(id -u)/hypr/ 2>/dev/null | head -1)}"
  # hyprland lua env may differ; also export for systemd user
  systemctl --user import-environment SAL_USE_VCLPLUGIN 2>/dev/null || true
fi

echo ""
echo "Restart LibreOffice to apply. Cells remain white; toolbars follow dark/MintVine."
