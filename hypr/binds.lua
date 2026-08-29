-- See https://wiki.hypr.land/Configuring/Basics/Binds/
local v = require("vars")

hl.bind(v.mainMod .. " + RETURN", hl.dsp.exec_cmd(v.terminal))
hl.bind(v.mainMod .. " + Q",      hl.dsp.window.close())
hl.bind(v.mainMod .. " + M",      hl.dsp.exit())
hl.bind(v.mainMod .. " + E",      hl.dsp.exec_cmd(v.fileManager))
hl.bind(v.mainMod .. " + T",      hl.dsp.window.float({ action = "toggle" }))
hl.bind(v.mainMod .. " + SPACE",  hl.dsp.exec_cmd(v.menu))
hl.bind(v.mainMod .. " + R",      hl.dsp.exec_cmd(v.runner))
hl.bind(v.mainMod .. " + P",      hl.dsp.window.pseudo())

hl.bind(v.mainMod .. " + left",  hl.dsp.focus({ direction = "left" }))
hl.bind(v.mainMod .. " + right", hl.dsp.focus({ direction = "right" }))
hl.bind(v.mainMod .. " + up",    hl.dsp.focus({ direction = "up" }))
hl.bind(v.mainMod .. " + down",  hl.dsp.focus({ direction = "down" }))

for i = 1, 10 do
    local key = i % 10
    hl.bind(v.mainMod .. " + " .. key,         hl.dsp.focus({ workspace = i }))
    hl.bind(v.mainMod .. " + SHIFT + " .. key, hl.dsp.window.move({ workspace = i }))
end

hl.bind(v.mainMod .. " + mouse_down", hl.dsp.focus({ workspace = "e+1" }))
hl.bind(v.mainMod .. " + mouse_up",   hl.dsp.focus({ workspace = "e-1" }))

hl.bind(v.mainMod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
hl.bind(v.mainMod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })

hl.bind(v.mainMod .. " + F",         hl.dsp.window.fullscreen({ mode = "maximized" }))
hl.bind(v.mainMod .. " + SHIFT + F", hl.dsp.window.fullscreen({ mode = "fullscreen" }))

hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("brightnessctl set 5%-"), { locked = true, repeating = true })
hl.bind("XF86MonBrightnessUp",   hl.dsp.exec_cmd("brightnessctl set +5%"), { locked = true, repeating = true })

hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd("wpctl set-volume -l 1.4 @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("XF86AudioLowerVolume", hl.dsp.exec_cmd("wpctl set-volume -l 1.4 @DEFAULT_AUDIO_SINK@ 5%-"), { locked = true, repeating = true })

hl.bind(v.mainMod .. " + V",     hl.dsp.exec_cmd("cliphist list | wofi --dmenu --width 750 --height 400 | cliphist decode | wl-copy"))
hl.bind(v.mainMod .. " + C",     hl.dsp.exec_cmd(v.browser))
hl.bind(v.mainMod .. " + Print", hl.dsp.exec_cmd('grim -g "$(slurp -d)" - | wl-copy'))
hl.bind(v.mainMod .. " + L",     hl.dsp.exec_cmd("hyprlock"))
