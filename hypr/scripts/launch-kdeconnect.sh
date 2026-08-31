#!/usr/bin/env bash
# KDE Connect daemon — uses org.kde.desktop until qqc2-breeze-style is installed.
set -euo pipefail

# shellcheck source=kde-qt-env.sh
source "${HOME}/.config/hypr/scripts/kde-qt-env.sh"

pkill -x kdeconnectd 2>/dev/null || true
sleep 0.3

exec env -u QT_QPA_PLATFORMTHEME \
  QT_QUICK_CONTROLS_STYLE="${KDE_QT_QUICK_STYLE}" \
  /usr/bin/kdeconnectd
