-- See https://wiki.hypr.land/Configuring/Basics/Autostart/

local function detect_gtk_theme()
    if os.execute("test -d /usr/share/themes/Breeze-Dark") == 0 then
        return "Breeze-Dark"
    end
    if os.execute("test -d /usr/share/themes/Breeze") == 0 then
        return "Breeze"
    end
    return "Adwaita"
end

local gtk_theme = detect_gtk_theme()
hl.env("GTK_THEME", gtk_theme)

hl.on("hyprland.start", function()
  local home = os.getenv("HOME")
  -- Theme env + gsettings before dbus activation (fixes GTK4 apps like pavucontrol)
  hl.exec_cmd(home .. "/.config/hypr/scripts/apply-desktop-theme.sh")
  gtk_theme = detect_gtk_theme()
  hl.env("GTK_THEME", gtk_theme)
  hl.exec_cmd(
    "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP "
      .. "QT_QPA_PLATFORMTHEME QT_QPA_PLATFORM QT_QUICK_CONTROLS_STYLE "
      .. "XCURSOR_THEME XCURSOR_SIZE GTK_THEME SAL_USE_VCLPLUGIN"
  )
  hl.exec_cmd("systemctl --user import-environment GTK_THEME QT_QPA_PLATFORMTHEME QT_QUICK_CONTROLS_STYLE SAL_USE_VCLPLUGIN 2>/dev/null || true")

  hl.exec_cmd("hyprpaper")
  hl.exec_cmd("mako")
  hl.exec_cmd("wl-paste --type text --watch cliphist store")
  hl.exec_cmd("wl-paste --type image --watch cliphist store")
  hl.exec_cmd("/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1")
  hl.exec_cmd(home .. "/.config/hypr/scripts/launch-kdeconnect.sh")
  hl.exec_cmd("hyprctl setcursor Oxygen_White 24")
  hl.exec_cmd(home .. "/.config/waybar/launch.sh")
  hl.exec_cmd(home .. "/.config/conky/Atria/start.sh")
end)

hl.env("XCURSOR_SIZE", "24")
hl.env("XCURSOR_THEME", "Oxygen_White")
hl.env("QT_QPA_PLATFORMTHEME", "qt6ct")
hl.env("QT_QPA_PLATFORM", "wayland")
hl.env("QT_QUICK_CONTROLS_STYLE", "org.kde.breeze")
hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
-- LibreOffice chrome via KDE MintVine palette (document cells stay white via LO prefs)
hl.env("SAL_USE_VCLPLUGIN", "kf6")
