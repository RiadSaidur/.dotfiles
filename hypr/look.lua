-- Look & feel — frosted glass + vine borders
-- Literals on purpose so this file works even if require("vine") fails.

hl.config({
    general = {
        gaps_in     = 3,
        gaps_out    = 5,
        border_size = 3,
        col = {
            -- Active: high-contrast purple spectrum (deep → violet → magenta → hot pink)
            active_border   = {
                colors = {
                    "rgba(2e1065ff)",
                    "rgba(6d28d9ff)",
                    "rgba(c026d3ff)",
                    "rgba(db2777ff)",
                    "rgba(f0abfcff)",
                },
                angle  = 45,
            },
            -- Unfocused: barely-there purple
            inactive_border = { colors = { "rgba(6d28d922)", "rgba(1a182455)" }, angle = 160 },
        },
        layout = "dwindle",
    },

    decoration = {
        rounding         = 10,
        active_opacity   = 0.92,
        inactive_opacity = 0.78,
        blur = {
            enabled           = true,
            size              = 12,
            passes            = 4,
            new_optimizations = true,
            xray              = false,
            ignore_opacity    = true,
            vibrancy          = 0.16,
            vibrancy_darkness = 0.12,
            -- Required for frosted GTK/Qt right-click menus (xdg-popup)
            popups            = true,
            popups_ignorealpha = 0.08,
        },
        shadow = {
            enabled        = true,
            range          = 8,
            render_power   = 3,
            offset         = { 0, 0 },
            color          = {
                colors = {
                    "rgba(6d28d9cc)",
                    "rgba(c026d3dd)",
                    "rgba(db2777cc)",
                },
                angle  = 135,
            },
            color_inactive = "rgba(6d28d910)",
        },
        -- No inner glow — halo is thin outward shadow only
        glow = {
            enabled = false,
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
