-- See https://wiki.hypr.land/Configuring/Basics/Window-Rules/
hl.window_rule({
    match          = { class = "^kitty$" },
    opacity        = "0.94 override 0.70 override",
    suppress_event = "maximize fullscreen",
})

-- Keep Firefox fully opaque so selection damage isn't lost under decoration opacity
hl.window_rule({
    match     = { class = "^firefox$" },
    workspace = "1",
    float     = false,
    opacity   = "1.0 override 1.0 override",
})

-- Chromium family: native Wayland menus can frost with blur:popups when translucent
hl.window_rule({
    match   = { class = "^(google-chrome|chromium|brave-browser|Brave-browser|chrome)$" },
    opacity = "0.95 override 0.86 override",
})

hl.window_rule({
    match     = { class = "^.*[Cc]ode.*$" },
    workspace = "2",
    opacity   = "0.84 override 0.74 override",
})

hl.window_rule({
    match   = { class = "^cursor$" },
    opacity = "0.84 override 0.74 override",
})

-- Android Emulator: MUST float. If tiled, Hyprland stretches the chrome into a
-- giant empty frame while the phone surface stays phone-sized (classic white void).
-- Match initial_* — title/class can change after map (see r/hyprland 1hmp81t, 1mn4yoc).
-- Real configs: float + title:^(Emulator)$ / title:^(Android Emulator -)
local function android_emulator_chrome(extra)
    local r = {
        opacity            = "1.0 override 1.0 override",
        opaque             = true,
        float              = true,
        center             = true,
        keep_aspect_ratio  = true,
        no_blur            = true,
        no_shadow          = true,
        no_dim             = true,
        force_rgbx         = true,
        immediate          = true,
        render_unfocused   = true,
        rounding           = 0,
        border_size        = 2,
        persistent_size    = true,
        suppress_event     = "maximize",
    }
    if extra then
        for k, v in pairs(extra) do
            r[k] = v
        end
    end
    return r
end

hl.window_rule(android_emulator_chrome({
    name  = "android-emulator-class",
    match = { class = "^(qemu-system-.*|Emulator)$" },
}))
hl.window_rule(android_emulator_chrome({
    name  = "android-emulator-initial-class",
    match = { initial_class = "^(qemu-system-.*|Emulator)$" },
}))
hl.window_rule(android_emulator_chrome({
    name  = "android-emulator-title",
    match = { title = "^(Android Emulator|Emulator)" },
}))
hl.window_rule(android_emulator_chrome({
    name  = "android-emulator-initial-title",
    match = { initial_title = "^(Android Emulator|Emulator)" },
}))

hl.window_rule({
    match   = { class = "^(polkit-gnome|Polkit-gnome-authentication-agent-1)$" },
    float   = true,
    center  = true,
    opacity = "0.92 override 0.92 override",
})
hl.window_rule({
    match   = { title = "^Authentication is required" },
    float   = true,
    center  = true,
    opacity = "0.92 override 0.92 override",
})

hl.window_rule({
    match = { class = "^com\\.interversehq\\.qView$" },
    float = true,
})

hl.window_rule({
    match   = { class = "^org\\.remmina\\.Remmina$" },
    opacity = "0.94 override 0.86 override",
})
hl.window_rule({
    match   = { class = "^org\\.remmina\\.Remmina$", title = "^Remote Connection Profile$" },
    float   = true,
    center  = true,
    opacity = "0.94 override 0.94 override",
})

hl.window_rule({
    match = { class = "^[Cc]onky$" },
    float = true,
    pin = true,
    no_focus = true,
    no_shadow = true,
    no_blur = true,
    border_size = 0,
})

hl.window_rule({
    match = { class = "^org\\.gnome\\.Nautilus$" },
    opacity = "0.84 override 0.72 override",
})

hl.window_rule({
    match = { class = "^org\\.kde\\.kdeconnect\\.app$" },
    opacity = "0.84 override 0.72 override",
})

-- GTK file/folder pickers (Cursor/VS Code/Firefox "Open/Add Folder" via portal)
hl.window_rule({
    match   = { class = "^xdg-desktop-portal-gtk$" },
    float   = true,
    center  = true,
    opacity = "0.84 override 0.84 override",
})
hl.window_rule({
    match   = { title = "^(Add Folder to Workspace|Open File|Open Folder|Save File|Select Folder|Choose Files?)$" },
    float   = true,
    center  = true,
    opacity = "0.84 override 0.84 override",
})

hl.layer_rule({
    match = { namespace = "waybar" },
    blur = true,
    ignore_alpha = 0.18,
})
hl.layer_rule({
    match = { namespace = "wofi" },
    blur = true,
    ignore_alpha = 0.15,
})
hl.layer_rule({
    match = { namespace = "notifications" },
    blur = true,
    ignore_alpha = 0.15,
})
hl.layer_rule({
    match = { namespace = "shadow-cat" },
    blur = false,
    ignore_alpha = 0.0,
})

hl.workspace_rule({
    workspace = "2",
    monitor   = "HDMI-A-2",
    default   = true,
})
