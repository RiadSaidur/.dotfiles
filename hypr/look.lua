-- Look & feel — frosted glass + vine borders
-- Literals on purpose so this file works even if require("vine") fails.

hl.config({
    general = {
        gaps_in     = 3,
        gaps_out    = 4,
        border_size = 4,
        col = {
            -- Active: multi-stop purple → violet → fuchsia → pink (not green / not gold)
            active_border   = {
                colors = {
                    "rgba(4c1d95ff)",
                    "rgba(7c3aedff)",
                    "rgba(a855f7ff)",
                    "rgba(d946efff)",
                    "rgba(e879f9ff)",
                },
                angle  = 35,
            },
            -- Unfocused: dim purple ember
            inactive_border = { colors = { "rgba(7c3aed28)", "rgba(1a182466)" }, angle = 160 },
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
            range          = 36,
            render_power   = 4,
            offset         = { 0, 0 },
            color          = {
                colors = {
                    "rgba(4c1d9588)",
                    "rgba(7c3aed99)",
                    "rgba(d946efaa)",
                    "rgba(e879f9aa)",
                },
                angle  = 125,
            },
            color_inactive = "rgba(7c3aed18)",
        },
        -- Inner glow disabled — halo is outward shadow only
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
