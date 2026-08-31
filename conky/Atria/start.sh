#!/bin/bash
killall conky 2>/dev/null
sleep 1
exec conky -c "$HOME/.config/conky/Atria/Atria.conf"
