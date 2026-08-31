#!/usr/bin/env bash
# Launch KDE/Kirigami apps with the correct Qt Quick style for this system.
set -euo pipefail

# shellcheck source=kde-qt-env.sh
source "${HOME}/.config/hypr/scripts/kde-qt-env.sh"

exec env -u QT_QPA_PLATFORMTHEME \
  QT_QUICK_CONTROLS_STYLE="${KDE_QT_QUICK_STYLE}" \
  "$@"
