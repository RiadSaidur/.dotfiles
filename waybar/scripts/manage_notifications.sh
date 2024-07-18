#!/bin/bash

# Menu options
options="View Notifications\nToggle DND\nClear All Notifications"

selected=$(echo -e "$options" | wofi --dmenu --prompt "Notifications")

case $selected in
    "View Notifications")
        pkill -SIGUSR1 mako  # This signal shows all pending notifications
        ;;
    "Toggle DND")
        ~/.config/waybar/scripts/toggle_dnd.sh
        ;;
    "Clear All Notifications")
        pkill -USR1 mako  # This signal clears all notifications
        ;;
esac
