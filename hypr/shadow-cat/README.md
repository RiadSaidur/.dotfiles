# shadow-cat

Orange-brained interactive desktop pet for **Hyprland**.

Sleeps most of the time. Wake it with hover or click — then expect
affection, vacant stares, and sudden diagonal zoomies. Built with GTK3,
gtk-layer-shell, and Cairo; talks to Hyprland over its Unix socket (read-only).

## Requirements

| Kind | Packages |
|------|----------|
| Build | `gtk3`, `gtk-layer-shell`, `pkgconf`, `gcc`, `make` |
| Runtime | `gtk3`, `gtk-layer-shell`, **Hyprland** session (for IPC) |

## Build

```bash
make
./shadow-cat
```

```bash
make test
sudo make install          # → /usr/bin/shadow-cat
sudo make uninstall
```

## Run under Hyprland

Add to your Hyprland config (see `packaging/hyprland.conf.example`):

```conf
exec-once = shadow-cat
layerrule = blur, unset, namespace:shadow-cat
layerrule = ignore_alpha 0.0, namespace:shadow-cat
```

Or from this rice tree (rebuilds when source is newer):

```bash
~/.config/hypr/shadow-cat/launch.sh
```

## Controls

| Input | Effect |
|-------|--------|
| Hover while sleeping | Soft wake |
| Left click | Affection (too many → dramatic flee) |
| Right / middle click | Random chaos stir |

## Version

```bash
shadow-cat --version
```

Current version: **0.1.0** (see `Makefile` `VERSION`).

## License

MIT — see [LICENSE](LICENSE).

## Packaging / AUR

Not published yet. When ready, follow **[packaging/AUR.md](packaging/AUR.md)**.
This directory is already shaped so it can become a standalone git repo
and an AUR `-git` (or release) package with little extra work.
