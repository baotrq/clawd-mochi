#!/usr/bin/env python3
"""
Clawd mascot frame processor.

Pipeline for a folder of video-export PNG frames (the orange Clawd mascot on a
dark/grey ASCII-art background).

  1. Background removal by COLOR KEY (simple, keeps the mascot's real pixels).
     The mascot is the only orange thing in the scene, so we mask "orange"
     pixels, keep the largest connected blob (drops any stray specks / the corner
     music note), fill enclosed holes so the dark eyes are kept, and write a
     transparent PNG cropped to the mascot. No grid quantization -- the sprite
     looks exactly like the source, just on transparency.
  2. (Optional) Duplicate removal on the centered cutout fingerprint.
  3. Numbered contact sheets so frames can be assigned to animations
     (typing / hop off chair / walking / hop on chair).

No ML model needed -- pure Pillow + numpy + scipy.

Usage:
    python clawd_frames_process.py \
        --src "../ezgif-40cff938d48ec142-png-split" \
        --out "../mascot_frames" --no-dedup
"""

import argparse
import os
import glob

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage


# --- color key ---------------------------------------------------------------

def orange_mask(rgb: np.ndarray) -> np.ndarray:
    """Boolean mask of the orange mascot body.

    The mascot measures ~(205,114,84); the background is grayscale (R≈G≈B), so
    "R noticeably above G and B" cleanly isolates it. Tuned from sampled frames.
    """
    R, G, B = rgb[..., 0].astype(int), rgb[..., 1].astype(int), rgb[..., 2].astype(int)
    return (R - G > 25) & (R - B > 40) & (R > 90)


# --- background removal ------------------------------------------------------

def cut_out_mascot(img: Image.Image, pad: int = 6):
    """Return (rgba_cropped, bbox) for the mascot, or (None, None) if not found.

    Keeps the mascot's original pixels; only the background is made transparent.
    """
    rgb = np.asarray(img.convert("RGB"))
    m = orange_mask(rgb)
    if m.sum() == 0:
        return None, None

    # Keep only the largest orange blob -> drops stray specks / music note.
    labels, n = ndimage.label(m)
    if n > 1:
        sizes = ndimage.sum(np.ones_like(labels), labels, index=range(1, n + 1))
        m = labels == (int(np.argmax(sizes)) + 1)

    # Fill enclosed holes so the dark eyes (holes inside the body) are kept,
    # then a 1px close to smooth single-pixel jaggies along the edge.
    m = ndimage.binary_fill_holes(m)
    m = ndimage.binary_closing(m, iterations=1)

    alpha = (m * 255).astype(np.uint8)
    rgba = np.dstack([rgb, alpha])

    ys, xs = np.where(m)
    x0, x1 = max(xs.min() - pad, 0), min(xs.max() + 1 + pad, rgba.shape[1])
    y0, y1 = max(ys.min() - pad, 0), min(ys.max() + 1 + pad, rgba.shape[0])
    crop = Image.fromarray(rgba[y0:y1, x0:x1], "RGBA")
    return crop, (x0, y0, x1, y1)


# --- duplicate detection (cutout fingerprint; only used when dedup is on) -----

def cutout_signature(crop: Image.Image, box: int = 128) -> np.ndarray:
    """Translation-invariant grayscale*alpha fingerprint of a mascot cutout."""
    canvas = Image.new("RGBA", (box, box), (0, 0, 0, 0))
    c = crop.copy()
    c.thumbnail((box, box), Image.NEAREST)
    canvas.paste(c, ((box - c.width) // 2, (box - c.height) // 2), c)
    a = np.asarray(canvas).astype(int)
    gray = a[..., :3].mean(2)
    return (gray * (a[..., 3] > 0)).astype(np.int16)


# --- contact sheet -----------------------------------------------------------

def build_contact_sheets(cutout_paths, out_dir, cols=8, thumb=180, label_h=22):
    os.makedirs(out_dir, exist_ok=True)
    try:
        font = ImageFont.truetype("arial.ttf", 14)
    except Exception:
        font = ImageFont.load_default()

    cell_w, cell_h = thumb, thumb + label_h
    per_sheet = cols * 6
    sheets = [cutout_paths[i:i + per_sheet] for i in range(0, len(cutout_paths), per_sheet)]

    for si, group in enumerate(sheets, 1):
        rows = (len(group) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (40, 40, 40))
        draw = ImageDraw.Draw(sheet)
        for idx, path in enumerate(group):
            r, c = divmod(idx, cols)
            x, y = c * cell_w, r * cell_h
            for by in range(0, thumb, 16):
                for bx in range(0, thumb, 16):
                    if (bx // 16 + by // 16) % 2 == 0:
                        draw.rectangle([x + bx, y + by, x + bx + 15, y + by + 15],
                                       fill=(70, 70, 70))
            im = Image.open(path).convert("RGBA")
            im.thumbnail((thumb, thumb), Image.NEAREST)
            ox = x + (thumb - im.width) // 2
            oy = y + (thumb - im.height) // 2
            sheet.paste(im, (ox, oy), im)
            name = os.path.splitext(os.path.basename(path))[0]
            draw.text((x + 4, y + thumb + 3), name, fill=(255, 255, 255), font=font)
        out = os.path.join(out_dir, f"contact_sheet_{si:02d}.png")
        sheet.save(out)
        print(f"  contact sheet: {out}  ({len(group)} frames)")


# --- main --------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="folder of source PNG frames")
    ap.add_argument("--out", required=True, help="output folder")
    ap.add_argument("--dup-tol", type=float, default=1.0,
                    help="cutout mean-abs-diff tolerance vs last kept frame "
                         "(0=exact only, higher=looser).")
    ap.add_argument("--no-dedup", action="store_true",
                    help="keep every frame; skip duplicate removal entirely.")
    args = ap.parse_args()

    src = os.path.abspath(args.src)
    out = os.path.abspath(args.out)
    cut_dir = os.path.join(out, "cutouts")
    sheet_dir = os.path.join(out, "contact_sheets")
    os.makedirs(cut_dir, exist_ok=True)

    frames = sorted(glob.glob(os.path.join(src, "*.png")))
    print(f"{len(frames)} source frames in {src}")

    last_sig = None
    cutout_paths = []
    n_dup = n_nomask = 0

    for path in frames:
        name = os.path.splitext(os.path.basename(path))[0]
        img = Image.open(path)

        crop, _ = cut_out_mascot(img)
        if crop is None:
            n_nomask += 1
            print(f"  NO MASCOT FOUND {name} (skipped)")
            continue

        if not args.no_dedup:
            sig = cutout_signature(crop)
            if last_sig is not None and np.abs(sig - last_sig).mean() <= args.dup_tol:
                n_dup += 1
                print(f"  dup    {name}")
                continue
            last_sig = sig

        dst = os.path.join(cut_dir, name + ".png")
        crop.save(dst)
        cutout_paths.append(dst)

    print(f"\nkept {len(cutout_paths)} cutouts | {n_dup} duplicates removed | "
          f"{n_nomask} no-mascot")
    print(f"cutouts -> {cut_dir}")

    print("\nbuilding contact sheets for labeling...")
    build_contact_sheets(cutout_paths, sheet_dir)

    print("\nNext: open the contact sheets, tell me the frame-number ranges for")
    print("  typing / hop_off_chair / walking / hop_on_chair, and I'll sort them.")


if __name__ == "__main__":
    main()
