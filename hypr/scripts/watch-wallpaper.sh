#!/usr/bin/env bash
# Watch ~/Downloads/bg.jpg and re-theme whenever it changes.
set -euo pipefail

HOME="${HOME:-/home/syds}"
WALL="${HOME}/Downloads/bg.jpg"
SCRIPTS="${HOME}/.config/hypr/scripts"
PIDFILE="${HOME}/.config/theme/wallpaper-watch.pid"
mkdir -p "$(dirname "${PIDFILE}")"

if [[ -f "${PIDFILE}" ]] && kill -0 "$(cat "${PIDFILE}")" 2>/dev/null; then
  echo "wallpaper-watch already running (pid $(cat "${PIDFILE}"))"
  exit 0
fi
echo $$ >"${PIDFILE}"
trap 'rm -f "${PIDFILE}"' EXIT

LAST_HASH=""
hash_wall() {
  [[ -f "${WALL}" ]] && sha256sum "${WALL}" | awk '{print $1}' || echo ""
}

apply() {
  sleep 0.4
  [[ -f "${WALL}" ]] || return 0
  local h
  h="$(hash_wall)"
  [[ -z "${h}" || "${h}" == "${LAST_HASH}" ]] && return 0
  LAST_HASH="${h}"
  echo "[wallpaper-watch] ${WALL} changed — re-theming"
  bash "${SCRIPTS}/set-wallpaper.sh" || true
}

LAST_HASH="$(hash_wall)"
echo "[wallpaper-watch] watching ${WALL}"

if command -v inotifywait >/dev/null 2>&1; then
  DIR="$(dirname "${WALL}")"
  BASE="$(basename "${WALL}")"
  while read -r file; do
    if [[ "${file}" == "${BASE}" ]]; then
      apply
    fi
  done < <(inotifywait -m -e close_write,moved_to,create --format '%f' "${DIR}")
else
  echo "[wallpaper-watch] inotify-tools not installed — polling every 2s"
  echo "                 sudo pacman -S --needed inotify-tools"
  while true; do
    apply
    sleep 2
  done
fi
