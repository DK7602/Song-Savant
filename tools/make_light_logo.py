#!/usr/bin/env python3
"""Song Savant: make the light theme's logo pictures from the dark theme's.

The owner chose one drawing for both themes (November 2026). The light theme shows the dark theme's owl and title
in deeper colours, without the soft glow (a glow is lost on a light background):

    python3 -I tools/make_light_logo.py        (from the repository's top folder)

reads Resources/owl.png and Resources/wordmark.png and writes Resources/owl-light.png and Resources/wordmark-light.png
(the same size, so the header draws both themes at exactly the same size and place).
"""
import os
import sys

import numpy as np
from PIL import Image


def deepen(path, value=0.52, saturation=1.25, glow_cut=80):
    a = np.asarray(Image.open(path).convert("RGBA")).astype(np.float32) / 255.0
    rgb, alpha = a[..., :3], a[..., 3]
    cut = glow_cut / 255.0
    alpha = np.clip((alpha - cut) / (1.0 - cut), 0.0, 1.0)          # only the solid strokes stay
    grey = rgb.mean(-1, keepdims=True)
    rgb = np.clip((grey + (rgb - grey) * saturation) * value, 0.0, 1.0)   # deeper, a little richer
    return Image.fromarray((np.dstack([rgb, alpha]) * 255.0 + 0.5).astype(np.uint8), "RGBA")


def main():
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")
    for name in ("owl", "wordmark"):
        out = deepen(os.path.join(res, name + ".png"))
        out.save(os.path.join(res, name + "-light.png"), optimize=True)
        print("wrote", name + "-light.png", out.size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
