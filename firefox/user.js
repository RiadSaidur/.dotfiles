// Mint-vine Firefox prefs — copied into the active profile by apply-firefox-theme.sh
user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
user_pref("browser.tabs.inTitlebar", 1);
user_pref("ui.systemUsesDarkTheme", 1);
user_pref("browser.theme.dark-private-windows", true);
user_pref("widget.gtk.rounded-bottom-corners.enabled", true);
user_pref("svg.context-properties.content.enabled", true);

// Wayland: prefer xdg_popup (move-to-rect) so Hyprland blur:popups can frost menus
user_pref("widget.wayland.use-move-to-rect", true);
// Mirror of force-move when present (Firefox 149+ builds); ignored if unknown
user_pref("widget.wayland.force-move-to-rect", true);

// Keep page view opaque — only toolbars/menus use translucent chrome CSS
user_pref("browser.tabs.allow_transparent_browser", false);
user_pref("widget.non-native-theme.use-theme-accent", false);
