#!/usr/bin/env bash
# Apply mint-vine colors to Google Chrome / Chromium / Brave.
# Prefer patching the *active* theme pack in-place so Chrome Sync / CWS
# can't silently restore Catppuccin charcoal on next launch.
set -euo pipefail

SRC="${HOME}/.config/chromium-themes/mint-vine"

if [[ ! -f "${SRC}/manifest.json" ]]; then
  echo "Missing ${SRC}/manifest.json"
  exit 1
fi

if pgrep -f 'google-chrome|chromium|brave-browser' >/dev/null 2>&1; then
  echo "Close Chrome/Chromium/Brave fully first (prefs get overwritten while running)."
  echo "  pkill -f google-chrome; then re-run this script."
  exit 1
fi

python3 - "${SRC}" <<'PY'
import json, os, struct, zlib, sys, time
from pathlib import Path

src = Path(sys.argv[1])
FRAME = (14, 28, 22)
PANEL = (26, 40, 34)
PANEL2 = (30, 52, 40)
INK = (230, 246, 242)
INK_STRONG = (244, 250, 246)
MUTED = (155, 184, 170)
LEAF = (107, 191, 136)
SAND = (196, 168, 130)

def write_png(path: Path, rgb, w=128, h=128):
    r, g, b = rgb
    raw = b"".join(b"\x00" + bytes([r, g, b]) * w for _ in range(h))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )

def theme_blob():
    return {
        "images": {
            "theme_frame": "images/frame.png",
            "theme_frame_inactive": "images/frame_inactive.png",
            "theme_toolbar": "images/toolbar.png",
            "theme_tab_background": "images/tab.png",
            "theme_ntp_background": "images/ntp.png",
        },
        "colors": {
            "frame": list(FRAME),
            "frame_inactive": [12, 24, 18],
            "frame_incognito": [16, 32, 24],
            "frame_incognito_inactive": [12, 24, 18],
            "toolbar": list(PANEL),
            "toolbar_button_icon": list(INK),
            "tab_text": list(INK_STRONG),
            "tab_background_text": list(MUTED),
            "tab_background_text_inactive": [120, 150, 136],
            "tab_background_text_incognito": list(MUTED),
            "tab_background_text_incognito_inactive": [120, 150, 136],
            "bookmark_text": list(SAND),
            "button_background": list(PANEL2),
            "omnibox_background": list(PANEL2),
            "omnibox_text": list(INK),
            "ntp_background": list(FRAME),
            "ntp_text": list(INK),
            "ntp_link": list(SAND),
            "ntp_header": list(PANEL),
        },
        "tints": {
            "buttons": [-1, -1, -1],
            "frame": [-1, -1, -1],
            "frame_inactive": [-1, -1, 0.42],
            "background_tab": [-1, 0.35, 0.28],
            "frame_incognito": [-1, -1, -1],
            "frame_incognito_inactive": [-1, -1, 0.42],
        },
        "properties": {
            "ntp_background_alignment": "center",
            "ntp_background_repeat": "no-repeat",
            "ntp_logo_alternate": 1,
        },
    }

def write_swatches(img_dir: Path):
    img_dir.mkdir(parents=True, exist_ok=True)
    write_png(img_dir / "frame.png", FRAME)
    write_png(img_dir / "toolbar.png", PANEL)
    write_png(img_dir / "tab.png", FRAME)
    write_png(img_dir / "ntp.png", FRAME)
    write_png(img_dir / "frame_inactive.png", (12, 24, 18))
    write_png(img_dir / "icon128.png", (45, 102, 68))

# Keep source theme fresh
src_manifest = json.loads((src / "manifest.json").read_text())
src_manifest["name"] = "Mint Vine"
src_manifest["version"] = src_manifest.get("version", "1.1.0")
src_manifest["theme"] = theme_blob()
write_swatches(src / "images")
(src / "manifest.json").write_text(json.dumps(src_manifest, indent=2) + "\n")

profiles = []
for browser in (
    Path.home() / ".config/google-chrome",
    Path.home() / ".config/chromium",
    Path.home() / ".config/BraveSoftware/Brave-Browser",
):
    default = browser / "Default"
    if default.is_dir():
        profiles.append((browser.name, default, browser / "Local State"))

if not profiles:
    print("No Chrome/Chromium/Brave profile found.")
    sys.exit(1)

for name, profile, local_state in profiles:
    prefs_path = profile / "Preferences"
    if not prefs_path.is_file():
        continue
    prefs = json.loads(prefs_path.read_text())
    theme = prefs.get("extensions", {}).get("theme", {})
    tid = theme.get("id")
    settings = prefs.setdefault("extensions", {}).setdefault("settings", {})

    # Patch active theme pack in-place when present (survives ID sticking to Catppuccin etc.)
    applied = False
    if tid and tid in settings:
        rel = settings[tid].get("path") or f"{tid}/"
        pack = profile / "Extensions" / rel
        if pack.is_dir():
            manifest_path = pack / "manifest.json"
            manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
            manifest["name"] = "Mint Vine"
            manifest["description"] = "Dark mint-vine frame/toolbar matching Firefox + Hyprland rice"
            manifest.pop("update_url", None)
            manifest["theme"] = theme_blob()
            if "manifest_version" not in manifest:
                manifest["manifest_version"] = 3
            if "version" not in manifest:
                manifest["version"] = "1.1.0"
            write_swatches(pack / "images")
            manifest_path.write_text(json.dumps(manifest, indent=3) + "\n")
            for pak in pack.glob("Cached Theme.pak"):
                pak.unlink()
            meta = pack / "_metadata"
            if meta.exists():
                import shutil
                shutil.rmtree(meta)

            entry = settings[tid]
            entry["manifest"] = manifest
            entry["from_webstore"] = False
            entry.pop("cws-info", None)
            entry["disable_reasons"] = []
            entry["last_update_time"] = str(int(time.time() * 1000))
            settings[tid] = entry
            prefs["extensions"]["theme"] = {
                "id": tid,
                "pack": str(pack),
                "system_theme": 0,
            }
            applied = True
            print(f"==> {name}: patched active theme pack ({tid})")

    if not applied:
        print(f"==> {name}: no active theme pack; load unpacked once:")
        print(f"    chrome://extensions → Developer mode → Load unpacked → {src}")

    prefs.setdefault("browser", {}).setdefault("theme", {})["follows_system_colors"] = False
    prefs["should_read_incoming_syncing_theme_prefs"] = False
    prefs.pop("autogenerated", None)
    tmp = str(prefs_path) + ".mintvine.tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(prefs, f, separators=(",", ":"), ensure_ascii=False)
    os.replace(tmp, prefs_path)

    if local_state.is_file():
        ls = json.loads(local_state.read_text())
        flags = [
            f for f in (ls.setdefault("browser", {}).get("enabled_labs_experiments") or [])
            if not f.startswith("ozone-platform-hint@")
        ]
        flags.append("ozone-platform-hint@2")
        ls["browser"]["enabled_labs_experiments"] = flags
        tmp = str(local_state) + ".mintvine.tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(ls, f, separators=(",", ":"), ensure_ascii=False)
        os.replace(tmp, local_state)
        print(f"==> {name}: Ozone platform = Wayland")

print("Open Chrome again — toolbar should be mint like Firefox.")
PY
