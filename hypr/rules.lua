-- See https://wiki.hypr.land/Configuring/Basics/Window-Rules/
hl.window_rule({
    match     = { class = "^firefox$" },
    workspace = "1",
    float     = false,
})

hl.window_rule({
    match     = { class = "^.*Code.*$" },
    workspace = "2",
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
    match   = { class = "^wofi$" },
    opacity = "0.5 override",
})

-- See https://wiki.hypr.land/Configuring/Basics/Workspace-Rules/
hl.workspace_rule({
    workspace = "2",
    monitor   = "HDMI-A-2",
    default   = true,
})
