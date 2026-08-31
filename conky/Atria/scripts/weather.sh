#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=weather-cache.sh
source "${SCRIPT_DIR}/weather-cache.sh"

CACHE_DIR="${HOME}/.cache/conky-atria"
CACHE_FILE="${CACHE_DIR}/weather.json"

ensure_cache
"${SCRIPT_DIR}/weather-render.sh" >/dev/null 2>&1 || true

if [[ ! -f "$CACHE_FILE" ]] || grep -q '"error"' "$CACHE_FILE" 2>/dev/null; then
    echo "—"
    exit 0
fi

temperature="$(read_field "$CACHE_FILE" '"temp":\K[^,]+')"
description="$(read_field "$CACHE_FILE" '"description":"\K[^"]+')"

if [[ -z "$temperature" ]]; then
    echo "—"
    exit 0
fi

rounded_temperature="$(printf "%.0f" "$temperature")"
echo "${rounded_temperature}°C  ·  ${description}"
