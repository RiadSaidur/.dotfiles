#!/usr/bin/env bash
# Install mint-vine Firefox chrome + enable userChrome.
set -euo pipefail

SRC_CHROME="${HOME}/.config/firefox/chrome/userChrome.css"
SRC_USERJS="${HOME}/.config/firefox/user.js"
PROFILES_INI="${HOME}/.mozilla/firefox/profiles.ini"

if [[ ! -f "${SRC_CHROME}" ]]; then
  echo "Missing ${SRC_CHROME}"
  exit 1
fi

profile=""
if [[ -f "${PROFILES_INI}" ]]; then
  profile="$(awk -F= '
    /^\[Install/ { in_install=1; next }
    /^\[/ { in_install=0 }
    in_install && $1=="Default" { print $2; exit }
  ' "${PROFILES_INI}")"
  if [[ -z "${profile}" ]]; then
    profile="$(awk -F= '
      /^Default=1$/ { want=1 }
      want && $1=="Path" { print $2; exit }
    ' "${PROFILES_INI}")"
  fi
fi

if [[ -z "${profile}" ]]; then
  echo "Could not find Firefox profile in ${PROFILES_INI}"
  exit 1
fi

# Path may be relative to ~/.mozilla/firefox
if [[ "${profile}" != /* ]]; then
  DEST="${HOME}/.mozilla/firefox/${profile}"
else
  DEST="${profile}"
fi

mkdir -p "${DEST}/chrome"
cp -f "${SRC_CHROME}" "${DEST}/chrome/userChrome.css"

# Merge prefs into user.js (overwrite our managed file; keeps profile prefs.js intact)
cp -f "${SRC_USERJS}" "${DEST}/user.js"

echo "==> Firefox mint-vine chrome installed"
echo "    Profile: ${DEST}"
echo "    Restart Firefox to apply."
echo ""
echo "Chrome/Chromium: run ${HOME}/.config/hypr/scripts/apply-chrome-theme.sh"
