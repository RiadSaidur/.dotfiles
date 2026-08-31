#!/bin/bash
# Shared OpenWeather cache for Atria conky (text + background icon).

CACHE_DIR="${HOME}/.cache/conky-atria"
CACHE_FILE="${CACHE_DIR}/weather.json"
CACHE_MAX_AGE=600

mkdir -p "$CACHE_DIR"

load_secrets() {
    local secrets="${HOME}/.config/conky/Atria/secrets"
    if [[ -f "$secrets" ]]; then
        # shellcheck source=/dev/null
        source "$secrets"
    fi
}

cache_is_stale() {
    [[ ! -f "$CACHE_FILE" ]] && return 0
    local age=$(( $(date +%s) - $(stat -c %Y "$CACHE_FILE" 2>/dev/null || echo 0) ))
    (( age > CACHE_MAX_AGE ))
}

fetch_weather() {
    load_secrets
    local api_key="${OPENWEATHER_API_KEY:-}"
    local city="${OPENWEATHER_CITY:-dhaka}"

    if [[ -z "$api_key" ]]; then
        printf '%s\n' '{"error":"no_key"}' >"$CACHE_FILE"
        return 1
    fi

    if ! curl -sf --max-time 8 \
        "https://api.openweathermap.org/data/2.5/weather?q=${city}&appid=${api_key}&units=metric" \
        >"$CACHE_FILE"; then
        printf '%s\n' '{"error":"fetch_failed"}' >"$CACHE_FILE"
        return 1
    fi
}

ensure_cache() {
    if cache_is_stale; then
        fetch_weather
    fi
}

read_field() {
    local file="$1" pattern="$2"
    grep -oP "$pattern" "$file" 2>/dev/null | head -1
}
