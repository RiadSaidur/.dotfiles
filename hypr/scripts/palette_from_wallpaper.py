#!/usr/bin/env python3
"""Extract a readable dark-field palette from a wallpaper and remap theme files.

Roles mirror the mint-vine intensity model:
  field/panel  — dark surfaces (~L of #0e1c16 / #12241c)
  leaf         — bright interactive accent
  leaf_deep    — stronger interactive / selection
  sand         — warm annotation (~3%)
  text/muted   — high-contrast ink on dark field
"""
from __future__ import annotations

import argparse
import colorsys
import json
import re
import shutil
import sys
from collections import Counter
from pathlib import Path

from PIL import Image

HOME = Path.home()
CONFIG = HOME / ".config"
THEME_DIR = CONFIG / "theme"
PALETTE_JSON = THEME_DIR / "palette.json"
WALLPAPER = HOME / "Downloads" / "bg.jpg"

# Intensity reference (first apply remaps these; later applies remap previous→new)
REFERENCE: dict[str, str] = {
    "field": "#0e1c16",
    "panel": "#12241c",
    "moss": "#1a2e24",
    "surface": "#1a2822",
    "surface2": "#162c22",
    "surface3": "#283a32",
    "surface4": "#2a3830",
    "surface5": "#243830",
    "surface6": "#1e2a24",
    "glass_top": "#2e4436",
    "text": "#e6f0ea",
    "text_strong": "#f4faf6",
    "muted": "#9bb8aa",
    "leaf": "#6bbf88",
    "leaf_deep": "#3d7a52",
    "leaf_mid": "#549d6c",
    "leaf_bright": "#7ad4a0",
    "leaf_hover": "#8fd4a8",
    "sand": "#c4a882",
    "sand_bright": "#d4b896",
    "sand_dim": "#9a8060",
    "kitty_bg": "#060e0a",
    "glow_ink": "#b4e6c4",
    "danger": "#b05454",
}

CONSUMERS = [
    "hypr/vine.lua",
    "waybar/colors.css",
    "wofi/style.css",
    "mako/config",
    "gtk-3.0/gtk.css",
    "gtk-4.0/gtk-mint-vine.css",
    "gtk-4.0/gtk.css",
    "firefox/chrome/userChrome.css",
    "kde-color-schemes/MintVine.colors",
    "qt6ct/colors/mint-vine.conf",
    "sddm/themes/where_is_my_sddm_theme/theme.conf",
    "sddm/themes/where_is_my_sddm_theme/Main.qml",
    "hypr/hyprlock.conf",
    "kitty/kitty.conf",
    "conky/Atria/Atria.conf",
]


def clamp(x: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return max(lo, min(hi, x))


def hex_to_rgb(h: str) -> tuple[int, int, int]:
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def rgb_to_hex(r: int, g: int, b: int) -> str:
    return f"#{r:02x}{g:02x}{b:02x}"


def rgb_to_hsv(r: int, g: int, b: int) -> tuple[float, float, float]:
    return colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)


def hsv_to_rgb(h: float, s: float, v: float) -> tuple[int, int, int]:
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, clamp(s), clamp(v))
    return int(round(r * 255)), int(round(g * 255)), int(round(b * 255))


def rel_luma(r: int, g: int, b: int) -> float:
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0


def mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))  # type: ignore[return-value]


def sample_wallpaper(path: Path, size: tuple[int, int] = (96, 54)) -> list[tuple[int, int, int]]:
    im = Image.open(path).convert("RGB")
    im = im.resize(size, Image.Resampling.BOX)
    raw = im.tobytes()
    return [(raw[i], raw[i + 1], raw[i + 2]) for i in range(0, len(raw), 3)]


def pick_accent_hue(pixels: list[tuple[int, int, int]]) -> tuple[float, float]:
    """Return (dominant_hue, sand_hue). Sand stays warm-gold for annotation contrast."""
    buckets: Counter[int] = Counter()
    warm: Counter[int] = Counter()
    for r, g, b in pixels:
        h, s, v = rgb_to_hsv(r, g, b)
        if s < 0.14 or v < 0.12 or v > 0.96:
            continue
        w = int(s * (1.0 - abs(v - 0.55) * 1.4) * 100)
        if w <= 0:
            continue
        key = int(h * 36) % 36
        buckets[key] += w
        if 0.04 <= h <= 0.13:
            warm[key] += w

    if not buckets:
        return 0.38, 0.09

    dom = max(buckets.items(), key=lambda kv: kv[1])[0] / 36.0
    sec = max(warm.items(), key=lambda kv: kv[1])[0] / 36.0 if warm else 0.09
    return dom, sec


def build_palette(path: Path) -> dict[str, str]:
    pixels = sample_wallpaper(path)
    accent_h, sand_h = pick_accent_hue(pixels)

    darks = [p for p in pixels if rel_luma(*p) < 0.45] or pixels
    avg = tuple(sum(c[i] for c in darks) // len(darks) for i in range(3))
    _ah, as_, _av = rgb_to_hsv(*avg)

    field = hsv_to_rgb(accent_h, clamp(as_ * 0.55 + 0.18, 0.22, 0.42), 0.10)
    panel = hsv_to_rgb(accent_h, clamp(as_ * 0.50 + 0.16, 0.20, 0.40), 0.13)
    moss = hsv_to_rgb(accent_h, clamp(as_ * 0.48 + 0.14, 0.18, 0.38), 0.16)
    surface = hsv_to_rgb(accent_h, clamp(as_ * 0.45 + 0.14, 0.16, 0.36), 0.15)
    surface2 = hsv_to_rgb(accent_h, clamp(as_ * 0.45 + 0.14, 0.16, 0.36), 0.14)
    surface3 = hsv_to_rgb(accent_h, clamp(as_ * 0.42 + 0.12, 0.14, 0.34), 0.20)
    surface4 = hsv_to_rgb(accent_h, clamp(as_ * 0.40 + 0.12, 0.14, 0.32), 0.21)
    surface5 = hsv_to_rgb(accent_h, clamp(as_ * 0.40 + 0.12, 0.14, 0.32), 0.19)
    surface6 = hsv_to_rgb(accent_h, clamp(as_ * 0.42 + 0.12, 0.14, 0.34), 0.17)
    glass_top = hsv_to_rgb(accent_h, clamp(as_ * 0.38 + 0.14, 0.16, 0.36), 0.24)
    kitty_bg = hsv_to_rgb(accent_h, clamp(as_ * 0.50 + 0.20, 0.20, 0.45), 0.055)

    leaf = hsv_to_rgb(accent_h, clamp(0.42 + as_ * 0.25, 0.40, 0.62), 0.74)
    leaf_deep = hsv_to_rgb(accent_h, clamp(0.38 + as_ * 0.20, 0.36, 0.55), 0.48)
    leaf_mid = mix(leaf_deep, leaf, 0.45)
    leaf_bright = hsv_to_rgb(accent_h, clamp(0.40 + as_ * 0.20, 0.35, 0.55), 0.84)
    leaf_hover = mix(leaf, leaf_bright, 0.55)

    sand = hsv_to_rgb(sand_h, 0.36, 0.78)
    sand_bright = hsv_to_rgb(sand_h, 0.32, 0.86)
    sand_dim = hsv_to_rgb(sand_h, 0.34, 0.58)

    text = hsv_to_rgb(accent_h, 0.08, 0.94)
    text_strong = hsv_to_rgb(accent_h, 0.04, 0.97)
    muted = hsv_to_rgb(accent_h, 0.18, 0.68)
    glow_ink = hsv_to_rgb(accent_h, 0.28, 0.88)

    roles = {
        "field": field,
        "panel": panel,
        "moss": moss,
        "surface": surface,
        "surface2": surface2,
        "surface3": surface3,
        "surface4": surface4,
        "surface5": surface5,
        "surface6": surface6,
        "glass_top": glass_top,
        "text": text,
        "text_strong": text_strong,
        "muted": muted,
        "leaf": leaf,
        "leaf_deep": leaf_deep,
        "leaf_mid": leaf_mid,
        "leaf_bright": leaf_bright,
        "leaf_hover": leaf_hover,
        "sand": sand,
        "sand_bright": sand_bright,
        "sand_dim": sand_dim,
        "kitty_bg": kitty_bg,
        "glow_ink": glow_ink,
        "danger": hex_to_rgb(REFERENCE["danger"]),
    }
    return {k: rgb_to_hex(*v) for k, v in roles.items()}


def load_previous() -> dict[str, str]:
    if PALETTE_JSON.is_file():
        data = json.loads(PALETTE_JSON.read_text())
        roles = data.get("roles") or data
        if isinstance(roles, dict) and "leaf" in roles:
            return {k: str(v).lower() for k, v in roles.items() if isinstance(v, str)}
    return {k: v.lower() for k, v in REFERENCE.items()}


def save_palette(path: Path, roles: dict[str, str]) -> None:
    THEME_DIR.mkdir(parents=True, exist_ok=True)
    PALETTE_JSON.write_text(
        json.dumps({"wallpaper": str(path.resolve()), "roles": {k: v.lower() for k, v in roles.items()}}, indent=2)
        + "\n"
    )
    lr, lg, lb = hex_to_rgb(roles["leaf"])
    fr, fg, fb = hex_to_rgb(roles["field"])
    lines = [
        "/* Auto-generated from wallpaper — do not edit */",
        f"@define-color ink {roles['text'].upper()};",
        f"@define-color ink-strong {roles['text_strong'].upper()};",
        f"@define-color muted {roles['muted'].upper()};",
        f"@define-color leaf {roles['leaf'].upper()};",
        f"@define-color leaf-deep {roles['leaf_deep'].upper()};",
        f"@define-color sand {roles['sand'].upper()};",
        f"@define-color sand-bright {roles['sand_bright'].upper()};",
        f"@define-color vine {roles['sand'].upper()};",
        f"@define-color glow rgba({lr}, {lg}, {lb}, 0.24);",
        f"@define-color glow-bright rgba({lr}, {lg}, {lb}, 0.38);",
        f"@define-color glow-deep rgba({fr}, {fg}, {fb}, 0.42);",
    ]
    (THEME_DIR / "colors.css").write_text("\n".join(lines) + "\n")


def rewrite_waybar(roles: dict[str, str]) -> None:
    lr, lg, lb = hex_to_rgb(roles["leaf"])
    fr, fg, fb = hex_to_rgb(roles["field"])
    (CONFIG / "waybar" / "colors.css").write_text(
        f"""/* Auto-generated from wallpaper — intensity matched to mint-vine reference */
@define-color ink {roles['text'].upper()};
@define-color ink-strong {roles['text_strong'].upper()};
@define-color muted {roles['muted'].upper()};
@define-color leaf {roles['leaf'].upper()};
@define-color leaf-deep {roles['leaf_deep'].upper()};
@define-color sand {roles['sand'].upper()};
@define-color sand-bright {roles['sand_bright'].upper()};
@define-color vine {roles['sand'].upper()};
@define-color glow rgba({lr}, {lg}, {lb}, 0.24);
@define-color glow-bright rgba({lr}, {lg}, {lb}, 0.38);
@define-color glow-deep rgba({fr}, {fg}, {fb}, 0.42);
"""
    )


def rewrite_vine_lua(roles: dict[str, str]) -> None:
    leaf = roles["leaf"].lstrip("#")
    deep = roles["leaf_deep"].lstrip("#")
    sand = roles["sand"].lstrip("#")
    bright = roles["sand_bright"].lstrip("#")
    moss = roles["moss"].lstrip("#")
    panel = roles["panel"].lstrip("#")
    text = roles["text"].lstrip("#")
    (CONFIG / "hypr" / "vine.lua").write_text(
        f"""-- Auto-generated from wallpaper (field + leaf interactive + sand annotation)
return {{
    mint       = "rgba({leaf}dd)",
    leaf       = "rgba({deep}cc)",
    leafBright = "rgba({leaf}ee)",
    sand       = "rgba({sand}cc)",
    sandSoft   = "rgba({sand}77)",
    sandBright = "rgba({bright}ee)",
    moss       = "rgba({moss}88)",
    panel      = "rgba({panel}ee)",
    text       = "rgba({text}ff)",
    inkHex     = "{roles['field']}",
    textHex    = "{roles['text'].upper()}",
    leafHex    = "{roles['leaf_deep'].upper()}",
    mintHex    = "{roles['leaf'].upper()}",
    sandHex    = "{roles['sand'].upper()}",
    sandBrightHex = "{roles['sand_bright'].upper()}",
    mossHex    = "{roles['moss'].upper()}",
    fieldHex   = "{roles['field']}",
    panelHex   = "{roles['panel']}",
}}
"""
    )


def write_chrome_palette_snippet(roles: dict[str, str]) -> None:
    def rgb(role: str) -> list[int]:
        return list(hex_to_rgb(roles[role]))

    (THEME_DIR / "chrome-palette.json").write_text(
        json.dumps(
            {
                "frame": rgb("field"),
                "panel": rgb("surface"),
                "panel2": rgb("moss"),
                "ink": rgb("text"),
                "ink_strong": rgb("text_strong"),
                "muted": rgb("muted"),
                "leaf": rgb("leaf"),
                "sand": rgb("sand"),
            },
            indent=2,
        )
        + "\n"
    )


def build_replacements(old: dict[str, str], new: dict[str, str]) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    for role, old_hex in old.items():
        if role not in new:
            continue
        o = old_hex.lower().lstrip("#")
        n = new[role].lower().lstrip("#")
        if o == n or len(o) != 6:
            continue
        pairs.append((f"#{o}", f"#{n}"))
        pairs.append((f"#{o.upper()}", f"#{n.upper()}"))
        pairs.append((f"#ff{o}", f"#ff{n}"))
        pairs.append((f"#FF{o.upper()}", f"#FF{n.upper()}"))
        for a in ("ff", "ee", "dd", "cc", "aa", "99", "88", "66"):
            pairs.append((f"#{o}{a}", f"#{n}{a}"))
        or_, og, ob = int(o[0:2], 16), int(o[2:4], 16), int(o[4:6], 16)
        nr, ng, nb = int(n[0:2], 16), int(n[2:4], 16), int(n[4:6], 16)
        pairs.append((f"{or_},{og},{ob}", f"{nr},{ng},{nb}"))
        pairs.append((f"{or_}, {og}, {ob}", f"{nr}, {ng}, {nb}"))
    pairs.sort(key=lambda p: len(p[0]), reverse=True)
    seen: set[str] = set()
    out: list[tuple[str, str]] = []
    for a, b in pairs:
        key = a.lower()
        if key in seen:
            continue
        seen.add(key)
        out.append((a, b))
    return out


def apply_replacements(text: str, pairs: list[tuple[str, str]]) -> str:
    for old, new in pairs:
        if old.startswith("#"):
            text = re.sub(re.escape(old), new, text, flags=re.IGNORECASE)
        else:
            text = text.replace(old, new)
    return text


def patch_consumers(old: dict[str, str], new: dict[str, str]) -> list[str]:
    pairs = build_replacements(old, new)
    touched: list[str] = []
    for rel in CONSUMERS:
        path = CONFIG / rel
        if not path.is_file():
            continue
        try:
            original = path.read_text(encoding="utf-8")
        except OSError:
            continue
        updated = apply_replacements(original, pairs)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            touched.append(rel)
    return touched


def cmd_extract(path: Path) -> int:
    print(json.dumps({"wallpaper": str(path), "roles": build_palette(path)}, indent=2))
    return 0


def cmd_apply(path: Path) -> int:
    if not path.is_file():
        print(f"Wallpaper not found: {path}", file=sys.stderr)
        return 1
    old = load_previous()
    new = build_palette(path)
    save_palette(path, new)
    rewrite_waybar(new)
    rewrite_vine_lua(new)
    write_chrome_palette_snippet(new)
    touched = patch_consumers(old, new)
    mint = CONFIG / "gtk-4.0" / "gtk-mint-vine.css"
    gtk = CONFIG / "gtk-4.0" / "gtk.css"
    if mint.is_file():
        shutil.copy2(mint, gtk)
        touched.append("gtk-4.0/gtk.css (copied)")
    print(f"Palette from {path}")
    print(f"  leaf={new['leaf']}  sand={new['sand']}  field={new['field']}")
    print(f"  updated {len(touched)} files")
    for t in touched:
        print(f"    - {t}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("command", choices=["extract", "apply"])
    ap.add_argument("wallpaper", nargs="?", default=str(WALLPAPER))
    args = ap.parse_args()
    path = Path(args.wallpaper).expanduser()
    if args.command == "extract":
        return cmd_extract(path)
    return cmd_apply(path)


if __name__ == "__main__":
    sys.exit(main())
