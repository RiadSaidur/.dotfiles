-- See https://wiki.hypr.land/Configuring/Basics/Window-Rules/
hl.window_rule({
    match   = { class = "^kitty$" },
    opacity = "0.94 override 0.92 override",
})

hl.window_rule({
    match     = { class = "^firefox$" },
    workspace = "1",
    float     = false,
    opacity   = "0.92 override 0.88 override",
})

-- Chromium family: native Wayland menus can frost with blur:popups when translucent
hl.window_rule({
    match   = { class = "^(google-chrome|chromium|brave-browser|Brave-browser|chrome)$" },
    opacity = "0.92 override 0.88 override",
})

hl.window_rule({
    match     = { class = "^.*[Cc]ode.*$" },
    workspace = "2",
    opacity   = "0.88 override 0.82 override",
})

hl.window_rule({
    match   = { class = "^cursor$" },
    opacity = "0.88 override 0.82 override",
})

hl.window_rule({
    match = { class = "^polkit-gnome$" },
    float = true,
})

hl.window_rule({
    match = { class = "^com\\.interversehq\\.qView$" },
    float = true,
})

hl.window_rule({
    match = { class = "^google-chrome$", title = "^Open File$" },
    float = true,
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
    opacity = "0.88 override 0.80 override",
})

hl.window_rule({
    match = { class = "^org\\.kde\\.kdeconnect\\.app$" },
    opacity = "0.88 override 0.80 override",
})

-- GTK file/folder pickers (Cursor/VS Code/Firefox "Open/Add Folder" via portal)
hl.window_rule({
    match   = { class = "^xdg-desktop-portal-gtk$" },
    float   = true,
    center  = true,
    opacity = "0.88 override 0.88 override",
})
hl.window_rule({
    match   = { title = "^(Add Folder to Workspace|Open File|Open Folder|Save File|Select Folder|Choose Files?)$" },
    float   = true,
    center  = true,
    opacity = "0.88 override 0.88 override",
})

hl.layer_rule({
    match = { namespace = "waybar" },
    blur = true,
    ignore_alpha = 0.1,
})
hl.layer_rule({
    match = { namespace = "wofi" },
    blur = true,
    ignore_alpha = 0.15,
})
hl.layer_rule({
    match = { namespace = "notifications" },
    blur = true,
    ignore_alpha = 0.1,
})

hl.workspace_rule({
    workspace = "2",
    monitor   = "HDMI-A-2",
    default   = true,
})
