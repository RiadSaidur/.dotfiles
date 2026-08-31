-- Look & feel — frosted glass + vine borders
-- Literals on purpose so this file works even if require("vine") fails.

hl.config({
    general = {
        gaps_in     = 4,
        gaps_out    = 6,
        border_size = 2,
        col = {
            -- Soft leaf → mint; keep alpha below opaque so borders don't glow into gaps
            active_border   = { colors = { "rgba(3d7a52cc)", "rgba(6bbf88dd)" }, angle = 45 },
            -- Cool glass tint (not warm beige) so inactive edges don't clash with wallpaper
            inactive_border = "rgba(1a2e2488)",
        },
        layout = "dwindle",
    },

    decoration = {
        rounding         = 10,
        active_opacity   = 0.90,
        inactive_opacity = 0.86,
        blur = {
            enabled           = true,
            size              = 10,
            passes            = 3,
            new_optimizations = true,
            xray              = false,
            ignore_opacity    = true,
            vibrancy          = 0.25,
            vibrancy_darkness = 0.1,
            -- Required for frosted GTK/Qt right-click menus (xdg-popup)
            popups            = true,
            popups_ignorealpha = 0.2,
        },
        shadow = {
            enabled      = true,
            range        = 12,
            render_power = 3,
            -- Cool near-black green; low alpha so no muddy halo in gaps
            color        = 0x55080f0c,
        },
    },

    animations = {
        enabled = true,
    },

    dwindle = {
        preserve_split = true,
    },

    xwayland = {
        force_zero_scaling = true,
    },

    misc = {
        disable_hyprland_logo    = true,
        force_default_wallpaper  = 0,
        disable_splash_rendering = true,
    },
})

hl.curve("myBezier", { type = "bezier", points = { { 0.05, 0.9 }, { 0.1, 1.05 } } })

hl.animation({ leaf = "windows",     enabled = true, speed = 4,  bezier = "myBezier" })
hl.animation({ leaf = "windowsOut",  enabled = true, speed = 3,  bezier = "default", style = "popin 85%" })
hl.animation({ leaf = "border",      enabled = true, speed = 8,  bezier = "default" })
-- Disabled: spinning gradient borders look like color bleed around translucent windows
hl.animation({ leaf = "borderangle", enabled = false })
hl.animation({ leaf = "fade",        enabled = true, speed = 3,  bezier = "default" })
hl.animation({ leaf = "workspaces",  enabled = true, speed = 4,  bezier = "default" })
