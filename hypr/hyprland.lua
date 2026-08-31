-- Hyprland 0.55+ Lua entrypoint
-- https://wiki.hypr.land/Configuring/Start/
-- If this file exists, Hyprland loads it instead of hyprland.conf (chosen once at startup).

require("monitors")
require("autostart")
require("look")
require("input")
require("binds")
require("rules")
