#!/usr/bin/env python3
"""Sort mascot cutouts into animation folders.

Copies cutouts/ezgif-frame-NNN.png into categorized/<animation>/<name>_##.png,
renumbered sequentially per category. Non-destructive (originals stay).

Ranges are 1-based inclusive frame numbers; anything not listed is dropped.
"""
import os, shutil, glob

CUT = os.path.join(os.path.dirname(__file__), "..", "mascot_frames", "cutouts")
OUT = os.path.join(os.path.dirname(__file__), "..", "mascot_frames", "categorized")

# (range, stride) -- stride>1 keeps every Nth frame to thin a long cycle
CATEGORIES = {
    "typing":         (range(1, 16),  1),   # 1-15
    "get_off_chair":  (range(16, 27), 1),   # 16-26
    "walking":        (range(27, 73), 2),   # 27-72, every 2nd -> ~23 frames
    "jumping_up":     (range(73, 80), 1),   # 73-79
}

def src_for(n):
    return os.path.join(CUT, f"ezgif-frame-{n:03d}.png")

def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    total = 0
    for cat, (rng, stride) in CATEGORIES.items():
        d = os.path.join(OUT, cat)
        os.makedirs(d, exist_ok=True)
        seq = 1
        for n in list(rng)[::stride]:
            s = src_for(n)
            if not os.path.exists(s):
                print(f"  MISSING {cat}: frame {n:03d}")
                continue
            dst = os.path.join(d, f"{cat}_{seq:02d}.png")
            shutil.copy2(s, dst)
            seq += 1
            total += 1
        print(f"{cat:14s} {seq-1:3d} frames -> {os.path.relpath(d)}")
    kept_n = set()
    for rng, stride in CATEGORIES.values():
        kept_n.update(list(rng)[::stride])
    n_src = len(glob.glob(os.path.join(CUT, '*.png')))
    dropped = [n for n in range(1, n_src + 1) if n not in kept_n]
    print(f"\ntotal sorted: {total} | not used: {len(dropped)} {dropped}")

if __name__ == "__main__":
    main()
