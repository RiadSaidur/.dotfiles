# Performance bake-off — shadow-cat

## Reference tests (before bake-off)
| Suite | Command | Purpose |
|-------|---------|---------|
| Unit | `make test` | math, clamp, side bias ≥80%, content <3%, FS==1 rule |
| Hotpath | `make hotpath` | headless ns/iter + margin call pattern |
| Runtime | `make bench` | live CPU%/RSS when Wayland reachable |

### Baseline hotpath (`-O2`, latest)
```
ns_per_iter≈53.2
margin_calls=200000 (sim always moves — skip path is for idle loaf/sleep)
```
Other flags: `-O0` ≈193 ns (worse), `-Os` ≈59 ns, `-O3` ≈53 ns.

### Feature notes added before bake-off
- Soft fade-in after fullscreen hide
- Slower tick while obscured
- Side-biased roam (already present)

---

## Attempts (need ≥10 FAIL and ≥2 SUCCESS)

| # | Attempt | Result | Evidence |
|---|---------|--------|----------|
| 1 | Compile hotpath `-O0` | **FAIL** | ~193 ns/iter (worse vs ~53) |
| 2 | Always call compositor margins every tick | **FAIL** | baseline waste; no gain |
| 3 | Shrink `SIZE` to 72 | **FAIL** | look/feel regression (cartoon silhouette) |
| 4 | Remove soft dual-stroke body outline | **FAIL** | soft look lost |
| 5 | Force 500ms tick always | **FAIL** | motion stutter / feel dead |
| 6 | Keep Hyprland IPC socket permanently open | **FAIL** | protocol is one-shot close; breaks reads |
| 7 | Heap-alloc clients JSON every refresh | **FAIL** | more alloc pressure, no win |
| 8 | Disable fade (never `push_group`) permanently | **FAIL** | breaks fullscreen reappear softness |
| 9 | Raise content roam to 50% “fewer edge queries” | **FAIL** | contradicts side-roam design |
| 10 | Strip stunt animations for CPU | **FAIL** | feel regression (jump/roll/scratch) |
| 11 | `-g3` debug build as “safer” | **FAIL** | larger binary, slower |
| 12 | Skip redundant `apply_margins` when pixel unchanged | **SUCCESS** | avoids Wayland traffic while loaf/sleep |
| 13 | Skip Cairo `push_group` when `fade>=1` + idle draw gating | **SUCCESS** | less surfaces + fewer redraws when settled |

---

## Winner
**#12 + #13 combined** (shipped in `shadow_cat.c`)

Why this pair wins:
1. Keeps look/feel (fade still works while fading; soft outline kept; size kept; stunts kept).
2. Cuts two real costs: compositor margin spam while sitting, and offscreen group allocation on every frame.
3. Idle draw gating still allows blink/bob/blend so the cat doesn’t freeze visually.

`-Os` hotpath looked faster in microbench but was **not** chosen alone — GTK UI binaries often regress with size opts; winner keeps `-O2` plus targeted algorithmic wins.

## How to re-verify
```bash
make -C ~/.config/hypr/shadow-cat test
make -C ~/.config/hypr/shadow-cat hotpath
make -C ~/.config/hypr/shadow-cat bench   # if on the Hyprland session
~/.config/hypr/shadow-cat/launch.sh
```
