#!/usr/bin/env python3
"""Runcat-style CPU cat for Waybar (no GNOME extension)."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

CONFIG = Path(__file__).with_name("runcat.json")
cfg = json.loads(CONFIG.read_text())

RUN_FRAMES: list[str] = cfg["run_frames"]
SLEEP_FRAMES: list[str] = cfg.get("sleep_frames", RUN_FRAMES[:1])
FPS_LOW: float = float(cfg.get("fps_low", 2.5))
FPS_HIGH: float = float(cfg.get("fps_high", 8))
MIN_DELAY: float = float(cfg.get("min_delay", 0.14))
MAX_DELAY: float = float(cfg.get("max_delay", 0.55))
SLEEP_DELAY: float = float(cfg.get("sleep_delay", 0.45))
SLEEP_THRESHOLD: int = int(cfg.get("sleep_threshold", 8))
SLEEP_AFTER: int = int(cfg.get("sleep_after", 3))
POLL_INTERVAL: float = float(cfg.get("poll_interval", 1.0))

prev_idle: int | None = None
prev_total: int | None = None
cpu_percent = 0
low_ticks = 0
run_idx = 0
sleep_idx = 0
last_poll = 0.0


def read_cpu() -> int:
    global prev_idle, prev_total, cpu_percent, last_poll

    now = time.monotonic()
    if now - last_poll < POLL_INTERVAL:
        return cpu_percent

    last_poll = now
    with open("/proc/stat", encoding="utf-8") as fh:
        parts = fh.readline().split()[1:]
    values = [int(v) for v in parts]
    idle = values[3] + (values[4] if len(values) > 4 else 0)
    total = sum(values)

    if prev_total is not None:
        idle_delta = idle - prev_idle
        total_delta = total - prev_total
        if total_delta > 0:
            cpu_percent = max(0, min(100, int(100 * (1 - idle_delta / total_delta))))

    prev_idle, prev_total = idle, total
    return cpu_percent


def state_class(pct: int, sleeping: bool) -> str:
    if sleeping:
        return "sleeping"
    if pct >= 70:
        return "high"
    if pct >= 30:
        return "medium"
    return "low"


def main() -> None:
    global low_ticks, run_idx, sleep_idx

    while True:
        pct = read_cpu()

        if pct < SLEEP_THRESHOLD:
            low_ticks += 1
        else:
            low_ticks = 0

        sleeping = low_ticks >= SLEEP_AFTER

        if sleeping:
            frame = SLEEP_FRAMES[sleep_idx % len(SLEEP_FRAMES)]
            sleep_idx += 1
            delay = SLEEP_DELAY
        else:
            frame = RUN_FRAMES[run_idx % len(RUN_FRAMES)]
            run_idx += 1
            # Ease-in curve: stays slower until CPU is actually high
            load = (pct / 100) ** 1.4
            delay = 1 / (FPS_LOW + (FPS_HIGH - FPS_LOW) * load)
            delay = max(MIN_DELAY, min(MAX_DELAY, delay))

        payload = {
            "text": frame,
            "tooltip": f"CPU {pct}%",
            "class": state_class(pct, sleeping),
        }
        print(json.dumps(payload), flush=True)
        time.sleep(delay)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
