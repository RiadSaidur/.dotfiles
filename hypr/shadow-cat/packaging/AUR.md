# Publishing shadow-cat to the AUR (later)

Do **not** publish until the project lives in its **own public git repo**
(GitHub/GitLab/Codeberg). AUR `-git` packages must fetch sources from a
stable URL, not from a private rice monorepo.

## Prerequisites checklist (done in-tree)

- [x] MIT `LICENSE`
- [x] `README.md` with deps / build / run
- [x] Versioned `Makefile` with `PREFIX` / `DESTDIR` `install` / `uninstall`
- [x] `PKGBUILD` for `shadow-cat-hyprland-git`
- [x] `.SRCINFO` (regenerate after PKGBUILD edits)
- [x] Example Hyprland snippet under `packaging/`
- [x] `shadow-cat --version`

## Steps when you are ready

### 1. Split into a dedicated repository

From the rice repo (example with `git subtree`):

```bash
cd ~/.config
git subtree split -P hypr/shadow-cat -b shadow-cat-split
mkdir -p ~/src/shadow-cat && cd ~/src/shadow-cat
git init
git pull ~/.config shadow-cat-split
# create empty public repo, then:
git remote add origin git@github.com:YOURUSER/shadow-cat.git
git push -u origin master   # or main
```

Or copy this directory into a new repo and push — history optional.

### 2. Edit packaging URLs

In `PKGBUILD`, set:

- `url=` to the public project page
- `source=(git+https://…)` to the public clone URL
- `maintainer` / `pkgdesc` if you want

Then regenerate metadata:

```bash
makepkg --printsrcinfo > .SRCINFO
```

### 3. Local package test

```bash
makepkg -f
namcap PKGBUILD *.pkg.tar.zst
makepkg -i    # or pacman -U …
shadow-cat --version
```

Smoke-test under Hyprland with `exec-once = shadow-cat`.

### 4. Optional release tarball package

For a non-git AUR package later:

```bash
make dist
# upload dist/shadow-cat-VERSION.tar.gz to a GitHub Release
# write a second PKGBUILD with source=($url/…/shadow-cat-$pkgver.tar.gz)
# and sha256sums=('…')
```

### 5. Publish to AUR

```bash
# once: ssh key on aur.archlinux.org account
git clone ssh://aur@aur.archlinux.org/shadow-cat-hyprland-git.git
cd shadow-cat-hyprland-git
# copy PKGBUILD + .SRCINFO only (not the whole app — AUR stores packaging)
# for -git packages, AUR repo = packaging only; source fetched by PKGBUILD
git add PKGBUILD .SRCINFO
git commit -m "Initial import: shadow-cat-hyprland-git 0.1.0"
git push
```

Keep the **AUR git repo** (packaging) separate from the **upstream app repo**.

### 6. Naming notes

| Name | Use |
|------|-----|
| `shadow-cat-hyprland-git` | VCS / rolling (recommended first) |
| `shadow-cat-hyprland` | Versioned releases later |

`provides` / `conflicts` are already wired in `PKGBUILD`.

## What not to do

- Do not push this rice monorepo URL into AUR `source=`.
- Do not commit built `shadow-cat` binaries to AUR or upstream.
- Do not ship private paths (`~/.config/…/launch.sh`) as the package entrypoint;
  system install uses `/usr/bin/shadow-cat`.
