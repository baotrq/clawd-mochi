#!/usr/bin/env python3
"""Build one looping transparent GIF per animation category.

Each category's per-frame cutouts (cropped to their own bbox) are composited onto
a shared canvas, bottom-center aligned so the feet stay put and the loop doesn't
jitter. Output: mascot_frames/gifs/<category>.gif
"""
import os, glob
from PIL import Image

CAT_DIR = os.path.join(os.path.dirname(__file__), "..", "mascot_frames", "categorized")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "mascot_frames", "gifs")
DURATION_MS = 90          # per-frame; ~11 fps
PAD = 8

def load(cat):
    return [Image.open(f).convert("RGBA")
            for f in sorted(glob.glob(os.path.join(CAT_DIR, cat, "*.png")))]

def to_palette(rgba):
    """RGBA -> P-mode frame with index 255 reserved as transparent."""
    alpha = rgba.getchannel("A")
    p = rgba.convert("RGB").quantize(colors=255, method=Image.MEDIANCUT)
    transparent = alpha.point(lambda a: 255 if a <= 128 else 0)
    p.paste(255, transparent.convert("L").point(lambda v: 255 if v else 0))
    p.info["transparency"] = 255
    return p

def build(cat):
    frames = load(cat)
    if not frames:
        print(f"  {cat}: no frames"); return
    W = max(f.width for f in frames) + 2 * PAD
    H = max(f.height for f in frames) + 2 * PAD
    pal = []
    for f in frames:
        canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        x = (W - f.width) // 2                 # center horizontally
        y = H - PAD - f.height                 # anchor to bottom
        canvas.paste(f, (x, y), f)
        pal.append(to_palette(canvas))
    out = os.path.join(OUT_DIR, f"{cat}.gif")
    pal[0].save(out, save_all=True, append_images=pal[1:], duration=DURATION_MS,
                loop=0, transparency=255, disposal=2, optimize=False)
    print(f"  {cat:14s} {len(frames):2d} frames  {W}x{H}  -> {os.path.relpath(out)}")

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for cat in sorted(os.listdir(CAT_DIR)):
        if os.path.isdir(os.path.join(CAT_DIR, cat)):
            build(cat)

if __name__ == "__main__":
    main()
