#!/usr/bin/env python3
"""Makes the Song Savant video tutorial from the pictures SSTests --tutorial took (adapted from Groove Grabber's).

    python3 tools/tutorial/make_video.py CAPTURE_DIR OUT.mp4 --piper PIPER --voice VOICE.onnx

CAPTURE_DIR holds frames.json, frames/*.png and beat.wav (the song's starter parts). The narration is spoken by Piper (an offline voice); use a
voice whose licence allows what you'll do with the video (LJSpeech is public domain). Needs ffmpeg, numpy, Pillow.
"""
import argparse, hashlib, json, math, os, re, subprocess, sys, wave
from multiprocessing import Pool

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from script import SCRIPT, TITLE, OUTRO_LINE, OUTRO_SAY, INTRO_SAY, DRAG_LABEL  # noqa: E402

W, H, FPS, SR = 1920, 1080, 30, 44100
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ORANGE, BLUE, PURPLE = (244, 197, 91), (61, 107, 255), (138, 77, 255)     # (ORANGE is Song Savant's gold)

# fonts installed on the machine (Song Savant draws with the system's own): a clean sans and a serif for titles
FONT_PATTERNS = {"sans": ["Inter:style=SemiBold", "DejaVu Sans:style=Bold"], "sansm": ["Inter:style=Medium", "DejaVu Sans"],
                 "serif": ["DejaVu Serif:style=Bold", "Liberation Serif:style=Bold"]}
_font_files = {}

def font(kind, size):
    if kind not in _font_files:
        path = ""
        for pat in FONT_PATTERNS[kind]:
            path = subprocess.run(["fc-match", "-f", "%{file}", pat], capture_output=True, text=True).stdout.strip()
            if path: break
        _font_files[kind] = path
    return ImageFont.truetype(_font_files[kind], size)

# ---------------------------------------------------------------- the words
ACRONYMS = {"DAW": "D.A.W.", "BPM": "B.P.M.", "MIDI": "middy", "WAV": "wave", "FX": "effects", "F1": "F one", "FAVS": "Favs"}

def spoken(text):
    """What the narrator says for a line written for the screen."""
    t = text.replace("808s", "eight-oh-eights").replace("808", "eight-oh-eight")
    t = t.replace("90s", "nineties").replace("50%", "fifty percent").replace("66%", "sixty-six percent")
    t = t.replace("Ctrl-click", "Control-click").replace("one-shot", "one shot")
    t = re.sub(r"\b88\b", "eighty-eight", t)
    def word(m):
        w = m.group(0)
        if w in ACRONYMS: return ACRONYMS[w]
        if len(w) > 1 and w.isupper(): return w.capitalize()      # UI words on screen in capitals: say them as words
        return w
    t = re.sub(r"[A-Za-z0-9]+", word, t)
    return t

def sentences(text):
    parts = re.split(r"(?<=[.!?])\s+", text.strip())
    return [p for p in parts if p]

# ---------------------------------------------------------------- the voice
def synth(piper, voice, text, cache):
    key = hashlib.sha1((voice + "|" + text).encode()).hexdigest()[:16]
    out = os.path.join(cache, key + ".wav")
    if not os.path.exists(out):
        raw = out + ".raw.wav"
        subprocess.run([piper, "-m", voice, "-f", raw, "--sentence-silence", "0.0", "--length-scale", "1.04"],
                       input=text.encode(), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-ar", str(SR), "-ac", "1", "-sample_fmt", "s16", out], check=True)
        os.remove(raw)
    with wave.open(out) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768.0
    # trim the silence piper leaves at the ends
    loud = np.where(np.abs(a) > 0.01)[0]
    if len(loud): a = a[max(0, loud[0] - 400): loud[-1] + 2000]
    return a

# ---------------------------------------------------------------- geometry
class Marks:
    def __init__(self, frames):
        self.frames = frames
        self.glob = {}
        for name, fr in frames.items():
            for k, r in fr["marks"].items():
                self.glob.setdefault(k, r)
        for name, fr in frames.items():         # derived: the export card (the panel covers the window), its steps
            m = fr["marks"]
            if "forBox" in m and "exportKit" in m:
                for k in ("exportToggles", "format"):
                    if k not in m and k in self.glob: m[k] = self.glob[k]     # (the same place on every export picture)
                f, e = m["forBox"], m["exportKit"]
                x0 = e[0] - 18; x1 = f[0] + f[2] + 18; y0 = f[1] - 150; y1 = e[1] + e[3] + 18
                m["exportCard"] = [x0, y0, x1 - x0, y1 - y0]
                if "exportToggles" in m:
                    t = m["exportToggles"]
                    m["steps"] = [x0 + 18, t[1] + t[3] + 12, x1 - x0 - 36, e[1] - (t[1] + t[3]) - 26]

    def get(self, key, *frames):
        for f in frames:
            if f and key in self.frames[f]["marks"]: return self.frames[f]["marks"][key]
        return self.glob.get(key)

def union(rects):
    rects = [r for r in rects if r]
    if not rects: return None
    x0 = min(r[0] for r in rects); y0 = min(r[1] for r in rects)
    x1 = max(r[0] + r[2] for r in rects); y1 = max(r[1] + r[3] for r in rects)
    return [x0, y0, x1 - x0, y1 - y0]

def camera_for(rect):
    """The view (in window coordinates) that frames a rectangle, 16:9, no more than 1.9x in."""
    if rect is None: return [0.0, 0.0, float(W), float(H)]
    x, y, w, h = rect
    pad = 0.16
    w2, h2 = w * (1 + 2 * pad), h * (1 + 2 * pad)
    w2 = max(w2, h2 * 16 / 9, W / 1.9)
    h2 = w2 * 9 / 16
    if h2 < h * 1.1: h2 = h * 1.1; w2 = h2 * 16 / 9
    w2, h2 = min(w2, W), min(h2, H)
    cx, cy = x + w / 2, y + h / 2
    x0 = min(max(cx - w2 / 2, 0), W - w2)
    y0 = max(cy - h2 / 2, y + h - 0.8 * h2)          # (the bottom of the view is where the subtitles go)
    y0 = min(max(y0, 0), H - h2)
    return [x0, y0, w2, h2]

def ease(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)

def lerp(a, b, t):
    return [a[i] + (b[i] - a[i]) * t for i in range(len(a))]

def centre(r):
    return (r[0] + r[2] / 2, r[1] + r[3] / 2)

# ---------------------------------------------------------------- the plan
MOVE, CLICK_AT = 0.75, 0.82          # the cursor's trip to a target, and when it clicks
DRAG = (90, 330)                     # where a drag takes the cursor (down, out of the pattern)

def plan(cap, marks, piper, voice, cache):
    """Every segment with its timing, shots, subtitles and narration audio."""
    segs, t = [], 0.0
    cursor = (W * 0.62, H * 0.72)
    cam = [0.0, 0.0, float(W), float(H)]
    for entry in SCRIPT:
        if "card" in entry:
            text = INTRO_SAY if entry["card"] == "title" else OUTRO_SAY
            lines = [(s, synth(piper, voice, spoken(s), cache)) for s in sentences(text)]
            dur = 1.2 + sum(len(a) / SR + 0.35 for _, a in lines) + (1.6 if entry["card"] == "outro" else 0.4)
            segs.append({"kind": entry["card"], "start": t, "dur": dur, "lines": lines, "lead": 1.0})
            t += dur
            continue
        if "chapter" in entry:
            segs.append({"kind": "chapter", "start": t, "dur": 2.2, "num": entry["chapter"], "title": entry["title"],
                         "frame": None, "lines": [], "lead": 0.0})
            t += 2.2
            continue
        lines = [(s, synth(piper, voice, spoken(s), cache)) for s in sentences(entry["say"])]
        speech = sum(len(a) / SR for _, a in lines) + 0.3 * (len(lines) - 1)
        shots = entry["shots"]
        lead = 0.45
        dur = max(lead + speech + 0.7, sum(max(1.2, 1.1 * s.get("w", 1)) for s in shots))
        if any("click" in s or "drag" in s for s in shots[:1]): dur += 0.3
        weights = [s.get("w", 1.0) for s in shots]
        total = sum(weights)
        st, out = 0.0, []
        for s, wgt in zip(shots, weights):
            d = dur * wgt / total
            f, pre = s["f"], s.get("pre")
            sh = dict(s, start=st, dur=d)
            target = None
            for k in ("click", "point", "drag"):
                if k in s:
                    r = marks.get(s[k], f, pre)
                    if r is None: print("  (no mark", s[k], "for", f, ")")
                    else: target = centre(r)
            sh["cursorFrom"], sh["cursorTo"] = cursor, target or cursor
            if target: cursor = target
            if "drag" in s: cursor = (cursor[0] + DRAG[0], cursor[1] + DRAG[1])
            zoom = s.get("zoom")
            new_cam = camera_for(union([marks.get(k, f, pre) for k in zoom])) if zoom else [0.0, 0.0, float(W), float(H)]
            sh["camFrom"], sh["camTo"] = cam, new_cam
            cam = new_cam
            sh["hlRects"] = [marks.get(k, f, pre) for k in s.get("hl", [])]
            sh["hlRects"] = [r for r in sh["hlRects"] if r]
            out.append(sh)
            st += d
        segs.append({"kind": "scene", "start": t, "dur": dur, "shots": out, "lines": lines, "lead": lead})
        t += dur
    # the chapter cards show the next scene's first frame behind them
    for i, s in enumerate(segs):
        if s["kind"] == "chapter":
            nxt = next((x for x in segs[i + 1:] if x["kind"] == "scene"), None)
            s["frame"] = (nxt["shots"][0].get("pre") or nxt["shots"][0]["f"]) if nxt else None
    # subtitles: each sentence while it's said
    for s in segs:
        subs, at = [], s["start"] + s["lead"]
        for text, a in s["lines"]:
            subs.append((at, at + len(a) / SR, text))
            at += len(a) / SR + (0.35 if s["kind"] in ("title", "outro") else 0.3)
        s["subs"] = subs
    return segs, t

# ---------------------------------------------------------------- the sound
def loudness_gain(path, target=-14.0):
    """dB to bring a file to `target` LUFS (integrated), as YouTube and most players play it."""
    r = subprocess.run(["ffmpeg", "-hide_banner", "-i", path, "-af", "ebur128", "-f", "null", "-"], capture_output=True, text=True)
    m = re.findall(r"I:\s+(-?[0-9.]+) LUFS", r.stderr)
    return target - float(m[-1]) if m else 0.0

def make_audio(segs, total, beat_path, out_path):
    n = int(total * SR) + SR
    voice = np.zeros(n, dtype=np.float32)
    for s in segs:
        for (t0, t1, _), (_, a) in zip(s["subs"], s["lines"]):
            i = int(t0 * SR)
            voice[i:i + len(a)] += a[: max(0, n - i)]
    peak = np.max(np.abs(voice)) or 1.0
    voice *= 0.89 / peak
    with wave.open(beat_path) as w:
        ch, rate = w.getnchannels(), w.getframerate()
        b = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32).reshape(-1, ch) / 32768.0
    if rate != SR: raise SystemExit("beat.wav should be 44.1 kHz")
    if ch == 1: b = np.repeat(b, 2, axis=1)
    reps = int(math.ceil(n / len(b)))
    music = np.tile(b, (reps, 1))[:n]
    # the music: up on the cards, down under the voice
    env = np.full(n, 0.12, dtype=np.float32)
    for s in segs:
        if s["kind"] in ("title", "outro", "chapter"):
            env[int(s["start"] * SR): int((s["start"] + s["dur"]) * SR)] = 0.30 if s["kind"] == "chapter" else 0.26
    talking = np.convolve((np.abs(voice) > 0.02).astype(np.float32), np.ones(int(0.25 * SR)) / int(0.25 * SR), mode="same") > 0.05
    env = np.where(talking, np.minimum(env, 0.095), env)
    k = int(0.35 * SR)
    env = np.convolve(env, np.ones(k) / k, mode="same").astype(np.float32)
    fade = int(2.5 * SR)
    env[-fade:] *= np.linspace(1, 0, fade)
    env[:int(0.3 * SR)] *= np.linspace(0, 1, int(0.3 * SR))
    mix = music * (env[:, None] * 1.0) + voice[:, None] * 0.95
    mix = np.tanh(mix * 1.05) / np.tanh(1.05)
    pcm = (np.clip(mix, -1, 1) * 32767).astype(np.int16)
    with wave.open(out_path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(pcm.tobytes())

# ---------------------------------------------------------------- the pictures
G = {}   # per worker: the plan, the pictures, the fonts

def worker_init(capdir, frames_json, segs, total):
    G.update(capdir=capdir, frames=frames_json["frames"], scale=frames_json["scale"], segs=segs, total=total, cache={})
    G["sub_font"] = font("sans", 44)
    G["cursor"] = make_cursor()
    G["cards"] = {}

def source(name):
    c = G["cache"]
    if name not in c:
        if len(c) > 8: c.pop(next(iter(c)))
        c[name] = Image.open(os.path.join(G["capdir"], G["frames"][name]["file"])).convert("RGB")
    return c[name]

def make_cursor():
    s = 4
    pts = [(0, 0), (0, 23), (5.6, 17.6), (9.6, 26.6), (13.4, 25), (9.4, 16.4), (16.6, 16.4)]
    img = Image.new("RGBA", (30 * s, 34 * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    P = [((x + 3) * s * 1.15, (y + 2) * s * 1.15) for x, y in pts]
    d.polygon(P, fill=(255, 255, 255, 255), outline=(10, 10, 14, 255), width=int(1.6 * s))
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).polygon([(x + 2 * s, y + 3 * s) for x, y in P], fill=(0, 0, 0, 120))
    shadow = shadow.filter(ImageFilter.GaussianBlur(2.5 * s))
    out = Image.alpha_composite(shadow, img)
    return out.resize((out.width // s, out.height // s), Image.LANCZOS)

def view(name, cam):
    sc = G["scale"]
    box = (cam[0] * sc, cam[1] * sc, (cam[0] + cam[2]) * sc, (cam[1] + cam[3]) * sc)
    return source(name).resize((W, H), Image.BILINEAR, box=box)

def to_screen(p, cam):
    k = W / cam[2]
    return ((p[0] - cam[0]) * k, (p[1] - cam[1]) * k)

def rect_to_screen(r, cam):
    (x0, y0), (x1, y1) = to_screen((r[0], r[1]), cam), to_screen((r[0] + r[2], r[1] + r[3]), cam)
    return [x0, y0, x1, y1]

def highlight(img, rects, alpha, cam):
    if alpha <= 0.01 or not rects: return img
    boxes = [rect_to_screen(r, cam) for r in rects]
    pad = 7
    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    for b in boxes: d.rounded_rectangle([b[0] - pad, b[1] - pad, b[2] + pad, b[3] + pad], radius=12, fill=255)
    dim = Image.eval(img, lambda v: int(v * 0.42))
    dimmed = Image.composite(img, dim, mask)
    img = Image.blend(img, dimmed, alpha)
    ring = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    for b in boxes:
        for grow, a, wid in ((6, 50, 8), (3, 110, 5), (0, 255, 3)):
            d.rounded_rectangle([b[0] - pad - grow, b[1] - pad - grow, b[2] + pad + grow, b[3] + pad + grow], radius=12 + grow,
                                outline=ORANGE + (int(a * alpha),), width=wid)
    img = img.convert("RGBA")
    img.alpha_composite(ring)
    return img.convert("RGB")

def draw_cursor(img, p, cam, ripple=None, drag_chip=False, label=None):
    x, y = to_screen(p, cam)
    rgba = img.convert("RGBA")
    if ripple is not None and 0 <= ripple <= 1:
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        r = 10 + 34 * ripple
        d.ellipse([x - r, y - r, x + r, y + r], outline=ORANGE + (int(255 * (1 - ripple)),), width=4)
        d.ellipse([x - 9, y - 9, x + 9, y + 9], fill=(255, 255, 255, int(110 * (1 - ripple))))
        rgba.alpha_composite(layer)
    if drag_chip:
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        f = G.setdefault("chip_font", font("sans", 26))
        label = label or DRAG_LABEL
        cw = f.getlength(label) + 40
        cx = x + 18 if x + 18 + cw < W - 10 else x - cw - 10
        d.rounded_rectangle([cx, y + 24, cx + cw, y + 74], radius=10, fill=(24, 26, 36, 240), outline=ORANGE + (255,), width=2)
        d.text((cx + 20, y + 33), label, font=f, fill=(240, 240, 245, 255))
        rgba.alpha_composite(layer)
    c = G["cursor"]
    rgba.alpha_composite(c, (int(x) - 3, int(y) - 2))
    return rgba.convert("RGB")

def subtitle(img, text, alpha, top=False):
    if not text or alpha <= 0.01: return img
    key = ("sub", text)
    if key not in G["cards"]:
        f = G["sub_font"]
        words, lines, cur = text.split(), [], ""
        for w_ in words:
            trial = (cur + " " + w_).strip()
            if f.getlength(trial) > 1460 and cur: lines.append(cur); cur = w_
            else: cur = trial
        lines.append(cur)
        lh = 54
        tw = max(f.getlength(l) for l in lines)
        bw, bh = int(tw + 64), int(lh * len(lines) + 28)
        box = Image.new("RGBA", (bw, bh), (0, 0, 0, 0))
        d = ImageDraw.Draw(box)
        d.rounded_rectangle([0, 0, bw - 1, bh - 1], radius=16, fill=(8, 10, 16, 238), outline=(255, 255, 255, 28), width=2)
        for i, l in enumerate(lines):
            d.text(((bw - f.getlength(l)) / 2, 10 + i * lh), l, font=f, fill=(246, 246, 250, 255))
        G["cards"][key] = box
    box = G["cards"][key]
    if alpha < 1:
        box = box.copy(); box.putalpha(box.getchannel("A").point(lambda v: int(v * alpha)))
    rgba = img.convert("RGBA")
    rgba.alpha_composite(box, ((W - box.width) // 2, 30 if top else H - box.height - 34))
    return rgba.convert("RGB")

def gradient_line(d, x0, x1, y, h):
    for x in range(int(x0), int(x1)):
        t = (x - x0) / max(1, x1 - x0)
        c = lerp(BLUE, PURPLE, t * 2) if t < 0.5 else lerp(PURPLE, ORANGE, (t - 0.5) * 2)
        d.line([(x, y), (x, y + h)], fill=tuple(int(v) for v in c))

def backdrop(name):
    key = ("bg", name)
    if key not in G["cards"]:
        if name: bg = view(name, [0, 0, W, H]).filter(ImageFilter.GaussianBlur(14))
        else: bg = Image.new("RGB", (W, H), (12, 14, 20))
        bg = Image.blend(bg, Image.new("RGB", (W, H), (6, 7, 12)), 0.68)
        G["cards"][key] = bg
    return G["cards"][key]

def card(seg):
    key = ("card", seg["kind"], seg.get("num"))
    if key in G["cards"]: return G["cards"][key]
    if seg["kind"] == "chapter":
        img = backdrop(seg["frame"]).copy()
        d = ImageDraw.Draw(img)
        f1, f2 = font("sans", 34), font("serif", 80)
        label = "STEP " + seg["num"]
        title = seg["title"].upper()
        d.text(((W - f1.getlength(label)) / 2, 410), label, font=f1, fill=ORANGE)
        tw = f2.getlength(title)
        if tw > W - 200: f2 = font("serif", int(80 * (W - 200) / tw)); tw = f2.getlength(title)
        d.text(((W - tw) / 2, 470), title, font=f2, fill=(244, 244, 248))
        gradient_line(d, W / 2 - 260, W / 2 + 260, 600, 4)
    else:
        img = backdrop("song-found").copy().convert("RGBA")
        owl = Image.open(os.path.join(REPO, "Resources", "owl.png")).convert("RGBA")
        owl = owl.resize((int(owl.width * 250 / owl.height), 250), Image.LANCZOS)
        img.alpha_composite(owl, ((W - owl.width) // 2, 130))
        word = Image.open(os.path.join(REPO, "Resources", "wordmark.png")).convert("RGBA")
        word = word.resize((660, int(word.height * 660 / word.width)), Image.LANCZOS)
        img.alpha_composite(word, ((W - word.width) // 2, 390))
        d = ImageDraw.Draw(img)
        fS, fV = font("serif", 50), font("sansm", 32)
        y = 390 + word.height + 18
        gradient_line(d, W / 2 - 420, W / 2 + 420, y, 4)
        line = TITLE if seg["kind"] == "title" else OUTRO_LINE
        d.text(((W - fS.getlength(line)) / 2, y + 26), line, font=fS, fill=ORANGE)
        sub = ("Version " + G["version"]) if seg["kind"] == "title" else "Press ? or F1 in Song Savant for the full manual"
        d.text(((W - fV.getlength(sub)) / 2, y + 102), sub, font=fV, fill=(178, 172, 210))
        img = img.convert("RGB")
    G["cards"][key] = img
    return img

def shot_at(seg, t):
    for sh in seg["shots"]:
        if t < sh["start"] + sh["dur"]: return sh
    return seg["shots"][-1]

def render(i):
    t = i / FPS
    seg = next((s for s in G["segs"] if t < s["start"] + s["dur"]), G["segs"][-1])
    lt = t - seg["start"]
    if seg["kind"] == "scene":
        sh = shot_at(seg, lt)
        st = lt - sh["start"]
        clicky = "click" in sh or "drag" in sh
        moves = sh["cursorFrom"] != sh["cursorTo"]
        cam = lerp(sh["camFrom"], sh["camTo"], ease(st / 0.9))
        name = sh["pre"] if (sh.get("pre") and st < CLICK_AT) else sh["f"]
        img = view(name, cam)
        start_hl = CLICK_AT + 0.15 if clicky else 0.25
        img = highlight(img, sh["hlRects"], ease((st - start_hl) / 0.4) * 0.9, cam)
        p = lerp(sh["cursorFrom"], sh["cursorTo"], ease(st / MOVE)) if moves else sh["cursorTo"]
        ripple, chip = None, False
        if "click" in sh: ripple = (st - CLICK_AT) / 0.45
        if "drag" in sh and st > CLICK_AT:
            k = ease((st - CLICK_AT - 0.2) / 1.6)
            p = (sh["cursorTo"][0] + DRAG[0] * k, sh["cursorTo"][1] + DRAG[1] * k)
            chip = True
        img = draw_cursor(img, p, cam, ripple, chip, sh.get("label"))
    else:
        img = card(seg)
    # subtitles (at the top while what's lit is down where they'd go)
    top = False
    if seg["kind"] == "scene":
        cam_now = lerp(sh["camFrom"], sh["camTo"], ease(st / 0.9))
        top = any(rect_to_screen(r, cam_now)[3] > H - 190 for r in sh["hlRects"])
    for (t0, t1, text) in seg["subs"]:
        if t0 - 0.12 <= t <= t1 + 0.25:
            a = min(1.0, (t - (t0 - 0.12)) / 0.12, (t1 + 0.25 - t) / 0.15)
            img = subtitle(img, text, a, top)
            break
    # dip to black around the cards, and at the very start and end
    fade = 1.0
    edge = 0.28
    idx = G["segs"].index(seg)
    prev_kind = G["segs"][idx - 1]["kind"] if idx > 0 else None
    nxt_kind = G["segs"][idx + 1]["kind"] if idx + 1 < len(G["segs"]) else None
    if seg["kind"] != "scene" or prev_kind != "scene": fade = min(fade, lt / edge)
    if seg["kind"] != "scene" or nxt_kind != "scene": fade = min(fade, (seg["dur"] - lt) / edge)
    fade = max(0.0, min(1.0, fade))
    if fade < 1: img = Image.blend(Image.new("RGB", (W, H), (0, 0, 0)), img, fade)
    return img.tobytes()

# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture"); ap.add_argument("out")
    ap.add_argument("--piper", required=True); ap.add_argument("--voice", required=True)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--preview", help="write single frames at these seconds (comma list) as PNGs instead of the video")
    ap.add_argument("--schedule", action="store_true", help="print when each part starts")
    ap.add_argument("--remix", action="store_true", help="only make the sound again, into the video OUT already is")
    a = ap.parse_args()
    fj = json.load(open(os.path.join(a.capture, "frames.json")))
    marks = Marks(fj["frames"])
    cache = os.path.join(a.capture, "tts")
    os.makedirs(cache, exist_ok=True)
    segs, total = plan(a.capture, marks, a.piper, a.voice, cache)
    print("  %d segments, %.1f s" % (len(segs), total))
    if a.schedule:
        for s in segs:
            what = s.get("title") or (s["subs"][0][2][:60] if s["subs"] else s["kind"])
            print("  %6.1f  %5.1f  %-8s %s" % (s["start"], s["dur"], s["kind"], what))
        return
    # what workers need (no audio arrays)
    lite = [{k: v for k, v in s.items() if k != "lines"} for s in segs]
    init = (a.capture, fj, lite, total)
    G["version"] = fj["info"].get("version", "")
    if a.preview:
        worker_init(*init); G["version"] = fj["info"].get("version", "")
        for sec in [float(x) for x in a.preview.split(",")]:
            p = a.out.replace(".mp4", "") + "-%06.2f.png" % sec
            Image.frombytes("RGB", (W, H), render(int(sec * FPS))).save(p)
            print("  wrote", p)
        return
    audio = a.out + ".wav"
    make_audio(segs, total, os.path.join(a.capture, "beat.wav"), audio)
    gain = loudness_gain(audio)
    level = ["-af", "volume=%.2fdB,alimiter=limit=0.89:level=false" % gain]
    if a.remix:
        tmp = a.out + ".remix.mp4"
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", a.out, "-i", audio, "-map", "0:v", "-map", "1:a", "-c:v", "copy"]
                       + level + ["-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", tmp], check=True)
        os.replace(tmp, a.out)
        os.remove(audio)
        print("  new sound in", a.out)
        return
    frames = int(total * FPS)
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H),
                           "-r", str(FPS), "-i", "-", "-i", audio] + level + ["-c:v", "libx264", "-preset", "medium", "-crf", "20",
                           "-pix_fmt", "yuv420p", "-tune", "stillimage", "-c:a", "aac", "-b:a", "192k", "-shortest",
                           "-movflags", "+faststart", a.out], stdin=subprocess.PIPE)
    with Pool(a.jobs, initializer=_init_with_version, initargs=init + (G["version"],)) as pool:
        for n, data in enumerate(pool.imap(render, range(frames), chunksize=8)):
            ff.stdin.write(data)
            if n % (FPS * 20) == 0: print("  %d / %d s" % (n // FPS, frames // FPS), flush=True)
    ff.stdin.close()
    ff.wait()
    os.remove(audio)
    print("  wrote", a.out)

def _init_with_version(capdir, fj, segs, total, version):
    worker_init(capdir, fj, segs, total)
    G["version"] = version

if __name__ == "__main__":
    main()
