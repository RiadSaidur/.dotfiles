#!/bin/bash
# Optional SVG→PNG renderer (mint-vine). Used by tools; Conky Wayland uses glyphs instead.
# Mapping: OpenWeather icon codes — same as erikflowers/weather-icons OWM list.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=weather-cache.sh
source "${SCRIPT_DIR}/weather-cache.sh"

CACHE_DIR="${HOME}/.cache/conky-atria"
CACHE_FILE="${CACHE_DIR}/weather.json"
OUT_PNG="${CACHE_DIR}/weather.png"
OUT_SVG="${CACHE_DIR}/weather.svg"
SIZE="${1:-160}"
COLOR="${2:-#3D7A52}"

ensure_cache

icon_code="04d"
if [[ -f "$CACHE_FILE" ]] && ! grep -q '"error"' "$CACHE_FILE" 2>/dev/null; then
    icon_code="$(read_field "$CACHE_FILE" '"icon":"\K[^"]+')"
    [[ -z "$icon_code" ]] && icon_code="04d"
fi

svg_for() {
    local code="$1" c="$COLOR"
    case "$code" in
        01d)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <circle cx="32" cy="32" r="12" fill="$c" opacity="0.85"/>
  <g stroke="$c" stroke-width="3" stroke-linecap="round" opacity="0.75">
    <path d="M32 8v6M32 50v6M8 32h6M50 32h6M14 14l4 4M46 46l4 4M50 14l-4 4M18 46l-4 4"/>
  </g>
</svg>
EOF
            ;;
        01n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <path fill="$c" opacity="0.85" d="M40 12a20 20 0 1 0 12 34 16 16 0 1 1-12-34z"/>
</svg>
EOF
            ;;
        02d|02n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <circle cx="22" cy="22" r="9" fill="$c" opacity="0.55"/>
  <path fill="$c" opacity="0.9" d="M20 40h28a10 10 0 0 0 0-20 12 12 0 0 0-23-3A9 9 0 0 0 20 40z"/>
</svg>
EOF
            ;;
        03d|03n|04d|04n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <path fill="$c" opacity="0.9" d="M18 42h30a11 11 0 0 0 0-22 14 14 0 0 0-26-4A10 10 0 0 0 18 42z"/>
</svg>
EOF
            ;;
        09d|09n|10d|10n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <path fill="$c" opacity="0.9" d="M18 34h30a11 11 0 0 0 0-22 14 14 0 0 0-26-4A10 10 0 0 0 18 34z"/>
  <g stroke="$c" stroke-width="3" stroke-linecap="round" opacity="0.8">
    <path d="M24 42v8M32 40v10M40 42v8"/>
  </g>
</svg>
EOF
            ;;
        11d|11n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <path fill="$c" opacity="0.85" d="M18 30h30a11 11 0 0 0 0-22 14 14 0 0 0-26-4A10 10 0 0 0 18 30z"/>
  <path fill="$c" d="M34 30l-8 14h7l-3 12 12-16h-7l5-10z"/>
</svg>
EOF
            ;;
        13d|13n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <g stroke="$c" stroke-width="3" stroke-linecap="round" opacity="0.9">
    <path d="M32 14v36M18 22l28 20M46 22L18 42"/>
  </g>
</svg>
EOF
            ;;
        50d|50n)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <g stroke="$c" stroke-width="3" stroke-linecap="round" opacity="0.75">
    <path d="M12 22h40M16 32h32M12 42h40"/>
  </g>
</svg>
EOF
            ;;
        *)
            cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <path fill="$c" opacity="0.9" d="M18 42h30a11 11 0 0 0 0-22 14 14 0 0 0-26-4A10 10 0 0 0 18 42z"/>
</svg>
EOF
            ;;
    esac
}

svg_for "$icon_code" >"$OUT_SVG"

if command -v rsvg-convert >/dev/null; then
    rsvg-convert -w "$SIZE" -h "$SIZE" "$OUT_SVG" -o "$OUT_PNG"
    printf '%s\n' "$OUT_PNG"
else
    exit 1
fi
