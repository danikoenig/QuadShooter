# Komposition Quad-Shooter-Reel: 1080x1920, 30 fps, GUI-Frames + Captions + Cursor + Endkarte
import json, math, subprocess, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H, FPS = 1080, 1920, 30
D = json.load(open("out/events.json"))
BEAT = D["beat"]; BAR = 4 * BEAT; TOTAL = D["total"]; GUI_UNTIL = D["guiUntil"]
R0, R1 = D["rects"], D["rectsAfter"]
FONT = "/usr/share/fonts/opentype/inter/"
def font(w, s): return ImageFont.truetype(FONT + {"black": "Inter-Black.otf", "bold": "Inter-Bold.otf", "med": "Inter-Medium.otf", "semi": "Inter-SemiBold.otf"}[w], s)

BG1, BG2 = (23, 24, 28), (15, 16, 19)
TEXT, DIM = (236, 238, 242), (125, 130, 140)
SLOT = [(255, 122, 47), (79, 195, 247), (105, 240, 174), (179, 136, 255)]
URL = "github.com/danikoenig/QuadShooter/releases"

def union(*rs):
    x0 = min(r[0] for r in rs); y0 = min(r[1] for r in rs)
    x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
    return [x0, y0, x1 - x0, y1 - y0]
def pad(r, p): return [r[0] - p, r[1] - p, r[2] + 2 * p, r[3] + 2 * p]

gui = R0["gui"]
slotsXY = pad(union(R0["slotA"], R0["slotD"], R0["xy"]), 10)
FOCUS = {
    "full":  [0, 0, gui[2], gui[3]],
    "drag":  pad(union(R0["library"], R0["slotA"], R0["slotD"]), 8),
    "xy":    pad(R0["xy"], 12),
    "tune":  pad(union(R0["xy"], R0["slotB"], R0["slotD"], R0["keySync"]), 10),
    "chrom": pad(union(R0["xy"], R0["slotB"], R0["slotD"], R0["chromatic"]), 10),
    "fx":    pad(union(R0["mDelay"], R0["mReverb"], R1["mDelay"], R1["mReverb"]), 10),
}
FOCUS["drag"][3] = R0["slotC"][1] + R0["slotC"][3] + 8 - FOCUS["drag"][1]   # unten bis Slot C

SCENES = [  # start, ende, titel, untertitel, fokus, farbe
    (0.0, 1 * BAR, "", "", "full", SLOT[0]),
    (1 * BAR, 3.5 * BAR, "DRAG 4 SOUNDS", "from the library into A · B · C · D", "drag", SLOT[0]),
    (3.5 * BAR, 6 * BAR, "BLEND 4 LAYERS", "one XY pad mixes them all", "xy", SLOT[1]),
    (6 * BAR, 8.5 * BAR, "TUNE EACH LAYER", "pitch detect · key sync · 1-cent slider", "tune", SLOT[2]),
    (8.5 * BAR, 10.5 * BAR, "PLAY IT CHROMATIC", "all four layers across your keys", "chrom", SLOT[3]),
    (10.5 * BAR, 13 * BAR, "DELAY · REVERB · CLIP", "chain them in any order", "fx", (77, 208, 225)),
]

def scene_at(t):
    for i, s in enumerate(SCENES):
        if s[0] <= t < s[1]: return i
    return len(SCENES) - 1

def ease(x): x = max(0.0, min(1.0, x)); return x * x * (3 - 2 * x)

BOX_W, BOX_TOP, BOX_MAXH = 1020, 590, 1100
def crop_at(t):
    i = scene_at(t)
    cur = FOCUS[SCENES[i][4]]
    if i == 0: return cur
    prev = FOCUS[SCENES[i - 1][4]]
    u = ease((t - SCENES[i][0]) / 0.5)
    return [prev[k] + (cur[k] - prev[k]) * u for k in range(4)]

def box_for(crop):
    s = BOX_W / crop[2]
    h = crop[3] * s
    if h > BOX_MAXH:
        s = BOX_MAXH / crop[3]; h = BOX_MAXH
    w = crop[2] * s
    x = (W - w) / 2
    y = BOX_TOP + (BOX_MAXH - h) * 0.32
    return x, y, w, h, s

# --- statischer Hintergrund
def make_bg():
    bg = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(bg)
    for y in range(H):
        u = y / H
        d.line([(0, y), (W, y)], fill=tuple(int(BG1[k] + (BG2[k] - BG1[k]) * u) for k in range(3)))
    glow = Image.new("RGB", (W, H), (0, 0, 0))
    g = ImageDraw.Draw(glow)
    for (cx, cy), c in zip([(0, 0), (W, 0), (0, H), (W, H)], SLOT):
        g.ellipse([cx - 520, cy - 520, cx + 520, cy + 520], fill=tuple(int(v * 0.16) for v in c))
    glow = glow.filter(ImageFilter.GaussianBlur(160))
    return Image.fromarray(__import__("numpy").clip(__import__("numpy").asarray(bg, dtype="int16") + __import__("numpy").asarray(glow, dtype="int16"), 0, 255).astype("uint8"))
BG = make_bg()

def logo(d, x, y, q, gap):
    for s in range(4):
        xx = x + (s % 2) * (q + gap); yy = y + (s // 2) * (q + gap)
        d.rounded_rectangle([xx, yy, xx + q, yy + q], radius=q * 0.25, fill=SLOT[s])

def centered(d, y, text, f, fill):
    w = d.textlength(text, font=f)
    d.text(((W - w) / 2, y), text, font=f, fill=fill)

# --- Cursor
CUR = D["cursor"]
def cursor_at(t):
    if not CUR or t < CUR[0][0]: return None
    for k in range(1, len(CUR)):
        if t <= CUR[k][0]:
            a, b = CUR[k - 1], CUR[k]
            u = ease((t - a[0]) / max(1e-6, b[0] - a[0]))
            return (a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u, bool(a[3]) and t < b[0] or bool(b[3]) and u > 0.999 or (bool(a[3]) and bool(b[3])))
    last = CUR[-1]
    return (last[1], last[2], bool(last[3]))
press_times = [CUR[k][0] for k in range(1, len(CUR)) if CUR[k][3] and not CUR[k - 1][3]]

def draw_cursor(img, x, y, pressed, t):
    d = ImageDraw.Draw(img, "RGBA")
    for pt in press_times:
        if 0 <= t - pt < 0.35:
            r = 14 + (t - pt) * 150; a = int(200 * (1 - (t - pt) / 0.35))
            d.ellipse([x - r, y - r, x + r, y + r], outline=(255, 255, 255, a), width=4)
    if pressed:
        d.ellipse([x - 20, y - 20, x + 20, y + 20], fill=(255, 255, 255, 60))
    s = 1.9
    pts = [(0, 0), (0, 22), (5.5, 17), (9.5, 26), (13, 24.5), (9, 16), (16, 16)]
    poly = [(x + px * s, y + py * s) for px, py in pts]
    d.polygon([(px + 2, py + 3) for px, py in poly], fill=(0, 0, 0, 110))
    d.polygon(poly, fill=(255, 255, 255, 255), outline=(20, 20, 24, 255))
    d.line(poly + [poly[0]], fill=(20, 20, 24, 255), width=3)

# --- Frames
frame_cache = {}
def gui_frame(f):
    return Image.open(f"out/frames/{f:04d}.jpg").convert("RGB")

F_CAP_T, F_CAP_S = font("black", 88), font("med", 40)
F_TITLE, F_TSUB = font("black", 122), font("semi", 44)
F_SMALL = font("semi", 34)
F_CHIP = font("bold", 26)

def render(f):
    t = f / FPS
    img = BG.copy()
    d = ImageDraw.Draw(img, "RGBA")

    if t < GUI_UNTIL + 0.6:
        gf = gui_frame(min(f, int(GUI_UNTIL * FPS) - 1))
        crop = crop_at(t)
        bx, by, bw, bh, s = box_for(crop)
        region = gf.crop((int(crop[0]), int(crop[1]), int(crop[0] + crop[2]), int(crop[1] + crop[3]))).resize((int(bw), int(bh)), Image.LANCZOS)
        i = scene_at(t)
        if i == 0:   # Titel: GUI abgedunkelt im Hintergrund
            region = Image.blend(region, Image.new("RGB", region.size, BG2), 0.55)
        # Schatten + Rahmen
        sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle([bx + 6, by + 18, bx + bw + 6, by + bh + 24], radius=26, fill=(0, 0, 0, 150))
        img.paste(sh.filter(ImageFilter.GaussianBlur(22)), (0, 0), sh.filter(ImageFilter.GaussianBlur(22)))
        mask = Image.new("L", region.size, 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, region.size[0] - 1, region.size[1] - 1], radius=22, fill=255)
        img.paste(region, (int(bx), int(by)), mask)
        d = ImageDraw.Draw(img, "RGBA")
        d.rounded_rectangle([bx, by, bx + bw, by + bh], radius=22, outline=(255, 255, 255, 30), width=2)

        # Caption
        sc = SCENES[i]
        if i == 0:
            u = ease(t / 0.5)
            logo(d, W / 2 - 68, 250 - 30 * (1 - u), 62, 12)
            centered(d, 420, "QUAD SHOOTER", F_TITLE, TEXT)
            centered(d, 570, "free 4-slot one-shot layering sampler", F_TSUB, DIM)
        else:
            u = ease((t - sc[0]) / 0.35)
            a = int(255 * u)
            yoff = 26 * (1 - u)
            d.rounded_rectangle([W / 2 - 40, 300 + yoff, W / 2 + 40, 308 + yoff], radius=4, fill=sc[5] + (a,))
            tw = d.textlength(sc[2], font=F_CAP_T)
            fcap = F_CAP_T if tw < W - 80 else font("black", int(88 * (W - 80) / tw))
            centered(d, 335 + yoff, sc[2], fcap, TEXT + (a,))
            centered(d, 450 + yoff, sc[3], F_CAP_S, DIM + (a,))
            # Marke oben
            logo(d, 72, 110, 22, 5)
            d.text((136, 112), "QUAD SHOOTER", font=font("black", 40), fill=TEXT)

        # Drag-Chips
        for chip in D["chips"]:
            t0, t1, name, si = chip
            if t0 <= t <= t1 + 0.05:
                c = cursor_at(t)
                if c:
                    cx = bx + (c[0] - crop[0]) * s; cy = by + (c[1] - crop[1]) * s
                    label = name.replace("WN_128_", "")
                    tw = d.textlength(label, font=F_CHIP)
                    x0, y0 = cx + 30, cy + 34
                    d.rounded_rectangle([x0, y0, x0 + tw + 56, y0 + 50], radius=14, fill=(29, 31, 36, 235), outline=SLOT[si] + (255,), width=3)
                    d.rounded_rectangle([x0 + 14, y0 + 17, x0 + 30, y0 + 33], radius=4, fill=SLOT[si])
                    d.text((x0 + 42, y0 + 10), label, font=F_CHIP, fill=TEXT)
        # Cursor
        c = cursor_at(t)
        if c and i > 0:
            cx = bx + (c[0] - crop[0]) * s; cy = by + (c[1] - crop[1]) * s
            if by - 40 < cy < by + bh + 40:
                draw_cursor(img, cx, cy, c[2], t)

        d = ImageDraw.Draw(img, "RGBA")
        centered(d, 1790, "free  ·  VST3 + AU  ·  Mac", F_SMALL, DIM)

    # Endkarte
    if t >= GUI_UNTIL:
        u = ease((t - GUI_UNTIL) / 0.6)
        end = BG.copy()
        e = ImageDraw.Draw(end, "RGBA")
        logo(e, W / 2 - 92, 430, 84, 16)
        centered(e, 660, "QUAD SHOOTER", font("black", 116), TEXT)
        centered(e, 805, "free 4-slot one-shot layering sampler", F_TSUB, DIM)
        lines = ["4 slots  ·  XY mix  ·  key sync", "delay  ·  reverb  ·  clipper  ·  limiter"]
        for k, ln in enumerate(lines):
            centered(e, 930 + k * 62, ln, font("semi", 40), TEXT)
        centered(e, 1110, "VST3 + AU  ·  Mac (Apple Silicon)", font("bold", 40), SLOT[1])
        fu = font("bold", 38)
        tw = e.textlength(URL, font=fu)
        bx0 = (W - tw) / 2 - 36
        e.rounded_rectangle([bx0, 1240, W - bx0, 1340], radius=24, fill=(29, 31, 36, 255), outline=SLOT[0] + (255,), width=3)
        centered(e, 1266, URL, fu, TEXT)
        centered(e, 1380, "free download  ·  link in bio", font("med", 36), DIM)
        centered(e, 1700, "by Dani Koenig", font("semi", 36), DIM)
        img = Image.blend(img, end, u) if u < 1 else end
    return img

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "preview":
        for ts in sys.argv[2:]:
            render(int(float(ts) * FPS)).save(f"/tmp/prev_{float(ts):05.2f}.jpg", quality=88)
        sys.exit(0)
    n = int(round(TOTAL * FPS))
    p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                          "-i", "-", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p", "video_tmp.mp4"],
                         stdin=subprocess.PIPE)
    for f in range(n):
        p.stdin.write(render(f).tobytes())
        if f % 150 == 0: print("compose", f, "/", n, flush=True)
    p.stdin.close(); p.wait()
    print("video done")
