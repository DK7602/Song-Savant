#!/usr/bin/env python3
"""Song Savant: make the light theme's logo pictures, matched to the dark theme's.

The owner's choice (November 2026): in the light theme, the owl keeps its filled-in body and the title keeps the
light artwork's colours (deep navy letters with blue and purple shine, gold swirls), but both are the same size and
in the same place as the dark theme's, and the title gets the dark title's underline with its little sound wave.

    python3 -I tools/make_light_logo.py        (from the repository's top folder)

Reads the light artwork cut-outs (docs/design/owl-light-cutout.png, wordmark-light-cutout.png) and the dark pictures
(Resources/owl.png, wordmark.png), and writes Resources/owl-light.png and wordmark-light.png on the dark pictures'
canvases, so the header draws both themes at exactly the same size and place.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

TOP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RES = os.path.join(TOP, "Resources")
DESIGN = os.path.join(TOP, "docs", "design")


def gold_box(img):
    """The owl's gold outline (ear tips, wings, bottom point): the part both owls share."""
    a = np.asarray(img.convert("RGBA")).astype(float)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    gold = (al > 200) & (r > 150) & (g > 110) & (b < 120) & (r > b + 60)
    ys, xs = np.nonzero(gold)
    return xs.min(), ys.min(), xs.max(), ys.max()


def solid_box(img, cut=120):
    al = np.asarray(img.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(al > cut)
    return xs.min(), ys.min(), xs.max(), ys.max()


def place(canvas, img, scale, src_centre, dst_centre, widen=1.0):
    w, h = round(img.width * scale * widen), round(img.height * scale)
    small = img.resize((w, h), Image.LANCZOS)
    x = round(dst_centre[0] - src_centre[0] * scale * widen)
    y = round(dst_centre[1] - src_centre[1] * scale)
    canvas.alpha_composite(small, (max(0, x), max(0, y)), (max(0, -x), max(0, -y)))


def make_owl():
    dark = Image.open(os.path.join(RES, "owl.png")).convert("RGBA")
    light = Image.open(os.path.join(DESIGN, "owl-light-cutout.png")).convert("RGBA")
    dx0, dy0, dx1, dy1 = gold_box(dark)
    lx0, ly0, lx1, ly1 = gold_box(light)
    # the same size: the gold outlines cover the same area (the light owl is a little taller and narrower)
    s = math.sqrt(((dx1 - dx0) * (dy1 - dy0)) / ((lx1 - lx0) * (ly1 - ly0)))
    out = Image.new("RGBA", dark.size, (0, 0, 0, 0))
    place(out, light, s, ((lx0 + lx1) / 2, (ly0 + ly1) / 2), ((dx0 + dx1) / 2, (dy0 + dy1) / 2))
    return out


def make_title():
    dark = Image.open(os.path.join(RES, "wordmark.png")).convert("RGBA")
    light = Image.open(os.path.join(DESIGN, "wordmark-light-cutout.png")).convert("RGBA")
    out = Image.new("RGBA", dark.size, (0, 0, 0, 0))

    # the dark title's underline and its little sound wave, in the light title's richer gold (no glow), moved down a
    # little to make room for the light letters
    under_top, drop = 139, 5
    a = np.asarray(dark).astype(np.float32) / 255.0
    region = np.zeros_like(a)
    region[under_top:, 140:421] = a[under_top:, 140:421]
    rgb, al = region[..., :3], region[..., 3]
    al = np.clip((al - 0.25) / 0.75, 0.0, 1.0)
    grey = rgb.mean(-1, keepdims=True)
    rgb = np.clip((grey + (rgb - grey) * 1.2) * 0.78, 0.0, 1.0)
    under = Image.fromarray((np.dstack([rgb, al]) * 255.0 + 0.5).astype(np.uint8), "RGBA")
    out.alpha_composite(under, (0, drop))

    # the light letters, the same size as the dark letters (the same area: they're a little taller and narrower),
    # centred where the dark letters are, and above the underline
    dx0, dy0, dx1, dy1 = 4, 3, 560, 130          # the dark letters (wordmarkContent() in Source/ui/Theme.cpp)
    lx0, ly0, lx1, ly1 = solid_box(light)
    s = math.sqrt(((dx1 - dx0) * (dy1 - dy0)) / ((lx1 - lx0) * (ly1 - ly0)))
    s = min(s, (under_top + drop - 3) / (ly1 - ly0))
    h = (ly1 - ly0) * s
    # a touch wider (6%, too little to look stretched) so it spans nearly the same width as the dark title
    place(out, light, s, ((lx0 + lx1) / 2, (ly0 + ly1) / 2), ((dx0 + dx1) / 2, h / 2 + 1), widen=1.06)
    return out


def main():
    for name, img in (("owl-light.png", make_owl()), ("wordmark-light.png", make_title())):
        img.save(os.path.join(RES, name), optimize=True)
        print("wrote", name, img.size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
