#!/usr/bin/env python3
# ============================================================
#  Nebula Engine - procedural asset generator
#  Produces the game's textures, launcher icons and SFX WAVs.
#  Re-run after edits:  python3 scripts/generate_assets.py
# ============================================================
import math
import os
import random
import struct
import wave

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX = os.path.join(ROOT, "app/src/main/assets/textures")
SOUNDS = os.path.join(ROOT, "app/src/main/assets/sounds")
RES = os.path.join(ROOT, "app/src/main/res")

os.makedirs(TEX, exist_ok=True)
os.makedirs(SOUNDS, exist_ok=True)

def rgba_image(size):
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


def glow(img, blur=6, alpha=180):
    """Add a soft glow behind the opaque parts of img."""
    glow_layer = img.filter(ImageFilter.GaussianBlur(blur))
    boosted = glow_layer.point(lambda p: p)
    combined = Image.alpha_composite(Image.new("RGBA", img.size, (0, 0, 0, 0)), boosted)
    combined = Image.alpha_composite(combined, img)
    return combined


# ---------------- Ship (player): sleek arrow with neon trim ----------------
def make_ship(path):
    S = 128
    img = rgba_image(S)
    d = ImageDraw.Draw(img)
    cx = S // 2
    # hull
    hull = [(cx, 6), (cx + 34, 52), (cx + 30, 92), (cx + 44, 112), (cx + 16, 100),
            (cx, 116), (cx - 16, 100), (cx - 44, 112), (cx - 30, 92), (cx - 34, 52)]
    d.polygon(hull, fill=(28, 38, 68, 255), outline=(94, 234, 255, 255))
    # canopy
    canopy = [(cx, 26), (cx + 13, 56), (cx, 78), (cx - 13, 56)]
    d.polygon(canopy, fill=(140, 240, 255, 255), outline=(230, 255, 255, 255))
    # wing accents
    d.line([(cx - 30, 60), (cx - 44, 108)], fill=(94, 234, 255, 255), width=4)
    d.line([(cx + 30, 60), (cx + 44, 108)], fill=(94, 234, 255, 255), width=4)
    # engine core
    d.ellipse([cx - 9, 96, cx + 9, 112], fill=(255, 196, 64, 255))
    img = glow(img, blur=5)
    img.save(path)


# ---------------- Enemy drone ----------------
def make_enemy(path):
    S = 128
    img = rgba_image(S)
    d = ImageDraw.Draw(img)
    cx = S // 2
    # menacing saucer
    d.ellipse([10, 44, S - 10, 92], fill=(64, 18, 44, 255), outline=(255, 92, 132, 255), width=4)
    d.ellipse([34, 22, S - 34, 66], fill=(92, 24, 60, 255), outline=(255, 140, 170, 255), width=3)
    # red eye
    d.ellipse([cx - 11, 40, cx + 11, 62], fill=(255, 64, 84, 255))
    d.ellipse([cx - 4, 47, cx + 4, 55], fill=(255, 220, 230, 255))
    # spines
    for side in (-1, 1):
        d.polygon([(cx + side * 46, 60), (cx + side * 62, 96), (cx + side * 28, 74)],
                  fill=(64, 18, 44, 255), outline=(255, 92, 132, 255))
    img = glow(img, blur=5)
    img.save(path)


# ---------------- Bullet (energy bolt) ----------------
def make_bullet(path):
    W, H = 32, 64
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([10, 2, 22, 62], radius=9, fill=(255, 236, 140, 255),
                        outline=(255, 176, 64, 255), width=2)
    d.rounded_rectangle([13, 10, 19, 54], radius=5, fill=(255, 255, 235, 255))
    img = glow(img, blur=4)
    img.save(path)


# ---------------- Star (soft sparkle) ----------------
def make_star(path):
    S = 32
    img = rgba_image(S)
    d = ImageDraw.Draw(img)
    c = S / 2
    d.ellipse([c - 3, c - 3, c + 3, c + 3], fill=(255, 255, 255, 255))
    d.line([(c, 0), (c, S)], fill=(255, 255, 255, 160), width=2)
    d.line([(0, c), (S, c)], fill=(255, 255, 255, 160), width=2)
    img = glow(img, blur=3)
    img.save(path)


# ---------------- Nebula background ----------------
def make_nebula(path):
    S = 1024
    random.seed(7)
    img = Image.new("RGB", (S, S))
    px = img.load()
    # vertical gradient: deep space blue -> near black
    top = (16, 20, 48)
    bottom = (3, 4, 12)
    for y in range(S):
        t = y / (S - 1)
        row = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for x in range(S):
            px[x, y] = row
    # nebula clouds (radial gradients of teal/magenta/purple)
    clouds = [((64, 224, 255), 0.05), ((190, 92, 255), 0.045), ((255, 92, 190), 0.035), ((80, 120, 255), 0.05)]
    overlay = Image.new("RGB", (S, S), (0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for color, strength in clouds:
        for _ in range(7):
            cx, cy = random.uniform(0, S), random.uniform(0, S)
            radius = random.uniform(120, 330)
            grad = Image.new("L", (S, S), 0)
            gd = ImageDraw.Draw(grad)
            gd.ellipse([cx - radius, cy - radius, cx + radius, cy + radius],
                       fill=int(255 * strength * random.uniform(0.6, 1.4)))
            tint = Image.new("RGB", (S, S), color)
            overlay = Image.composite(Image.blend(overlay, tint, 0.5), overlay, grad)
    img = Image.blend(img, Image.blend(img, overlay, 0.55), 1.0)
    # stars
    d = ImageDraw.Draw(img)
    for _ in range(420):
        x, y = random.uniform(0, S), random.uniform(0, S)
        b = random.choice([60, 110, 170, 230, 255])
        r = random.choice([0, 0, 0, 1, 1, 2])
        d.ellipse([x - r, y - r, x + r, y + r], fill=(b, b, b))
    img.save(path)


# ---------------- App logo / launcher emblem ----------------
def draw_emblem(img, scale=1.0):
    """Draws the planet emblem centered on a transparent image."""
    S = img.size[0]
    d = ImageDraw.Draw(img)
    cx, cy = S / 2, S / 2
    # planet ring
    ring_rx, ring_ry = S * 0.31, S * 0.115
    d.ellipse([cx - ring_rx, cy - ring_ry, cx + ring_rx, cy + ring_ry],
              outline=(120, 235, 255, 255), width=max(2, int(S * 0.028)))
    # planet body
    pr = S * 0.16
    planet = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    pd = ImageDraw.Draw(planet)
    pd.ellipse([cx - pr, cy - pr, cx + pr, cy + pr], fill=(94, 224, 255, 255))
    pd.ellipse([cx - pr * 0.35, cy - pr, cx + pr * 1.3, cy + pr], fill=(24, 120, 190, 255))
    planet_rot = planet.rotate(25, resample=Image.BICUBIC, center=(cx, cy))
    img.alpha_composite(planet_rot)
    d = ImageDraw.Draw(img)
    # orbiting moon + sparks
    d.ellipse([cx + ring_rx - S * 0.028, cy - S * 0.024,
               cx + ring_rx + S * 0.028, cy + S * 0.024], fill=(255, 214, 102, 255))
    for (sx, sy, sr) in [(0.18, 0.22, 6), (0.82, 0.28, 5), (0.24, 0.76, 5), (0.78, 0.74, 7)]:
        x, y = S * sx, S * sy
        r = sr * S / 512.0
        d.line([(x - r * 2, y), (x + r * 2, y)], fill=(255, 255, 255, 220), width=max(2, int(S * 0.006)))
        d.line([(x, y - r * 2), (x, y + r * 2)], fill=(255, 255, 255, 220), width=max(2, int(S * 0.006)))


def make_logo(path, size=512):
    S = size
    img = rgba_image(S)
    # rounded-square gradient background
    radius = int(S * 0.22)
    for y in range(S):
        t = y / (S - 1)
        color = (int(11 + 14 * t), int(16 + 12 * t), int(38 + 30 * t), 255)
        ImageDraw.Draw(img).line([(0, y), (S, y)], fill=color)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=radius, fill=255)
    img.putalpha(mask)
    draw_emblem(img)
    img.save(path)


def make_emblem(path, size=432):
    """Adaptive-icon foreground: emblem on transparency, safe-zone padded."""
    S = size
    inner = int(S * 0.72)
    emblem = rgba_image(inner)
    draw_emblem(emblem)
    img = rgba_image(S)
    img.alpha_composite(emblem, ((S - inner) // 2, (S - inner) // 2))
    img.save(path)


# ---------------- Sound effects (16-bit PCM mono WAV) ----------------
def write_wav(path, samples, rate=22050):
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(frames)


def sfx_laser():
    rate = 22050
    n = int(rate * 0.14)
    out = []
    for i in range(n):
        t = i / rate
        f = 1500 * math.exp(-14 * t) + 320
        env = math.exp(-9 * t)
        out.append(0.5 * env * math.sin(2 * math.pi * f * t))
    return out


def sfx_explosion():
    rate = 22050
    random.seed(1234)
    n = int(rate * 0.65)
    out = []
    last = 0.0
    for i in range(n):
        t = i / rate
        noise = random.uniform(-1, 1)
        last = 0.72 * last + 0.28 * noise          # cheap lowpass
        env = math.exp(-4.5 * t)
        thump = 0.6 * math.sin(2 * math.pi * 68 * t) * math.exp(-3.2 * t)
        out.append(0.65 * env * last + thump)
    return out


def sfx_hit():
    rate = 22050
    n = int(rate * 0.28)
    out = []
    for i in range(n):
        t = i / rate
        env = math.exp(-13 * t)
        out.append(0.6 * env * math.sin(2 * math.pi * 140 * t) +
                   0.25 * env * math.sin(2 * math.pi * 90 * t + 0.7))
    return out


def sfx_click():
    rate = 22050
    n = int(rate * 0.05)
    return [0.35 * math.exp(-90 * t) * math.sin(2 * math.pi * 2100 * t)
            for t in (i / rate for i in range(n))]


def sfx_gameover():
    rate = 22050
    notes = [392.0, 311.0, 233.0, 155.0]
    out = []
    for f in notes:
        seg = int(rate * 0.32)
        for i in range(seg):
            t = i / rate
            env = min(1.0, t * 24) * math.exp(-2.6 * t)
            out.append(0.42 * env * math.sin(2 * math.pi * f * t))
    return out


def main():
    make_ship(os.path.join(TEX, "ship.png"))
    make_enemy(os.path.join(TEX, "enemy.png"))
    make_bullet(os.path.join(TEX, "bullet.png"))
    make_star(os.path.join(TEX, "star.png"))
    make_nebula(os.path.join(TEX, "nebula_bg.png"))
    make_logo(os.path.join(RES, "drawable", "logo.png"), 512)

    # Adaptive-icon foreground (emblem only, transparent padding) & legacy sizes
    make_emblem(os.path.join(RES, "drawable", "ic_launcher_foreground.png"), 432)
    for density, size in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]:
        d = os.path.join(RES, f"mipmap-{density}")
        os.makedirs(d, exist_ok=True)
        make_logo(os.path.join(d, "ic_launcher.png"), size)
        make_logo(os.path.join(d, "ic_launcher_round.png"), size)

    write_wav(os.path.join(SOUNDS, "laser.wav"), sfx_laser())
    write_wav(os.path.join(SOUNDS, "explosion.wav"), sfx_explosion())
    write_wav(os.path.join(SOUNDS, "hit.wav"), sfx_hit())
    write_wav(os.path.join(SOUNDS, "click.wav"), sfx_click())
    write_wav(os.path.join(SOUNDS, "gameover.wav"), sfx_gameover())

    print("assets generated:")
    for dirpath, _, files in os.walk(os.path.join(ROOT, "app/src/main/assets")):
        for f in sorted(files):
            fp = os.path.join(dirpath, f)
            print(f"  {os.path.relpath(fp, ROOT)}  ({os.path.getsize(fp)} bytes)")


if __name__ == "__main__":
    main()
