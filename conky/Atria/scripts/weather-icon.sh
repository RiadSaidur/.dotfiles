#!/bin/bash
# Weather glyph for Atria — OpenWeather icon codes (01d…50n).
# Only uses codepoints present in JetBrainsMonoNL Nerd Font (f000–f385).
# Missing Material glyphs (f73d etc.) used to render as hex tofu (e.g. F7 / 3D).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=weather-cache.sh
source "${SCRIPT_DIR}/weather-cache.sh"

CACHE_DIR="${HOME}/.cache/conky-atria"
CACHE_FILE="${CACHE_DIR}/weather.json"

ensure_cache

if [[ ! -f "$CACHE_FILE" ]] || grep -q '"error"' "$CACHE_FILE" 2>/dev/null; then
    printf '\uf0c2'
    exit 0
fi

icon_code="$(read_field "$CACHE_FILE" '"icon":"\K[^"]+')"

case "$icon_code" in
    01d) printf '\uf185' ;;      # sun
    01n) printf '\uf186' ;;      # moon
    02d|02n) printf '\uf0c2' ;; # cloud (partly cloudy)
    03d|03n) printf '\uf0c2' ;; # cloud
    04d|04n) printf '\uf0c2' ;; # overcast
    09d|09n) printf '\uf043' ;; # droplet / showers
    10d|10n) printf '\uf043' ;; # droplet / rain
    11d|11n) printf '\uf0e7' ;; # bolt
    13d|13n) printf '\uf2dc' ;; # snowflake
    50d|50n) printf '\uf0c2' ;; # fog → soft cloud
    *) printf '\uf0c2' ;;
esac
