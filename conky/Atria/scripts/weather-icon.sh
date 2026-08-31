#!/bin/bash
# Weather glyph for Atria — OpenWeather icon codes (01d…50n).
# Canonical maps:
#   https://github.com/erikflowers/weather-icons  (wi-owm-* API list)
#   https://openweathermap.org/weather-conditions
#   Official PNGs: https://openweathermap.org/img/wn/{code}@4x.png

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
    01d) printf '\uf185' ;;
    01n) printf '\uf186' ;;
    02d) printf '\uf6c4' ;;
    02n) printf '\uf6c3' ;;
    03d|03n) printf '\uf0c2' ;;
    04d|04n) printf '\uf0c2' ;;
    09d|09n) printf '\uf740' ;;
    10d|10n) printf '\uf73d' ;;
    11d|11n) printf '\uf0e7' ;;
    13d|13n) printf '\uf2dc' ;;
    50d|50n) printf '\uf75f' ;;
    *) printf '\uf0c2' ;;
esac
