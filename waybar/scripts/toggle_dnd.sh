#!/bin/bash
dnd_file="/tmp/mako_dnd"

if [ -f "$dnd_file" ]; then
    rm "$dnd_file"
    notify-send "DND Disabled"
else
    touch "$dnd_file"
    notify-send "DND Enabled"
fi
