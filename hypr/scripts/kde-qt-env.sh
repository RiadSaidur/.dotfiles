#!/usr/bin/env bash
# Shared Qt Quick style for KDE/Kirigami apps (sourced, not executed).
if pacman -Qi qqc2-breeze-style &>/dev/null 2>&1; then
  KDE_QT_QUICK_STYLE="org.kde.breeze"
else
  KDE_QT_QUICK_STYLE="org.kde.desktop"
fi
