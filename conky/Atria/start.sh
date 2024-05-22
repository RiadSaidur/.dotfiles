#!/bin/bash
killall conky
sleep 2s
conky -c $HOME/.config/conky/Atria/Atria.conf
exit
