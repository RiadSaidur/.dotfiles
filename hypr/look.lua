-- Look & feel — frosted glass + vine borders
-- Literals on purpose so this file works even if require("vine") fails.

hl.config({
    general = {
        gaps_in     = 4,
        gaps_out    = 6,
        border_size = 2,
        col = {
            -- Leaf → mint: interactive / focused only (~7%)
            active_border   = { colors = { "rgba(3d7a52cc)", "rgba(6bbf88dd)" }, angle = 45 },
            -- Soft sand: annotation edge on unfocused (~3%)
            inactive_border = "rgba(c4a88255)",
        },
        layout = "dwindle",
    },

    decoration = {
        rounding         = 10,
        active_opacity   = 0.93,
        inactive_opacity = 0.90,
        blur = {
            enabled           = true,
            size              = 10,
            passes            = 3,
            new_optimizations = true,
            xray              = false,
            ignore_opacity    = true,
            -- Lower vibrancy: green wallpaper was tinting every translucent surface
            vibrancy          = 0.12,
            vibrancy_darkness = 0.15,
            -- Required for frosted GTK/Qt right-click menus (xdg-popup)
            popups            = true,
            popups_ignorealpha = 0.2,
        },
        shadow = {
            enabled      = true,
            range        = 12,
            render_power = 3,
            color        = 0x550a0a09,
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
