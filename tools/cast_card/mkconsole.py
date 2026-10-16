"""Top-view Dreamcast artwork for the launch animation (src/ui/launch_scene.c).
Three 256x256 textures in one coordinate space so the lid sits on the body:
  body.tex    RGB565    console with the lid off: plateau seams, tray well, spindle, buttons
  lid.tex     ARGB1555  the round lid: logo lock-up, front wedge (transparent outside)
  lid_in.tex  ARGB1555  underside of the lid (seen once it swings past edge-on)
Drawn at 4x and downsampled. Reference: top-down photos of a VA1 NTSC unit
(square shell, big round lid, logo in the upper third of the lid, grey wedge at the
front of the lid, POWER left / OPEN right, raised dots along both sides of the lid)."""
import os, math
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
TTF = os.path.join(ROOT, "assets", "fonts", "Dreamcast.ttf")
LOCKUP = os.path.join(ROOT, "assets", "brand", "dreamcast_lockup.png")   # swirl + Dreamcast (tools/cast_card/mkbrand.py)
S = 4                                   # supersample
N = 256 * S
BODY = (8, 10, 248, 246)                # shell rectangle in the 256 space
LID_C, LID_R = (128, 122), 96           # lid circle; hinge on its top edge (y = 26), bottom at 218
SHELL, SHELL_DK, SEAM = (233, 232, 227), (198, 196, 190), (205, 203, 197)
GREY_BTN, GREY_WEDGE, GREY_TEXT = (214, 213, 207), (156, 156, 152), (118, 118, 114)

def P(*v): return tuple(x * S for x in v)
def circ(d, c, r, **kw): d.ellipse([(c[0] - r) * S, (c[1] - r) * S, (c[0] + r) * S, (c[1] + r) * S], **kw)

def _vgrad(size, c0, c1):
    w, h = size
    g = Image.linear_gradient("L").resize((w, h))
    return Image.composite(Image.new("RGB", (w, h), c1), Image.new("RGB", (w, h), c0), g)

def _radial(size, c_in, c_out):
    w, h = size
    g = Image.radial_gradient("L").resize((w, h))
    return Image.composite(Image.new("RGB", (w, h), c_out), Image.new("RGB", (w, h), c_in), g)

def _wedge(d, y_top, y_apex, half, fill):
    d.polygon([P(128 - half, y_top), P(128 + half, y_top), P(128, y_apex)], fill=fill)

def body():
    img = Image.new("RGB", (N, N), (0, 0, 0))
    mask = Image.new("L", (N, N), 0); ImageDraw.Draw(mask).rounded_rectangle(P(*BODY), radius=20 * S, fill=255)
    img.paste(_vgrad((N, N), (240, 239, 234), (224, 223, 217)), (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle(P(*BODY), radius=20 * S, outline=SHELL_DK, width=int(1.5 * S))
    # plateau seams: four diagonals from the lid rim out to the shell edge
    for ang in (225, 315, 145, 35):
        a = math.radians(ang); dx, dy = math.cos(a), math.sin(a)
        ts = [t for t in ((BODY[0] - LID_C[0]) / dx if dx < 0 else (BODY[2] - LID_C[0]) / dx,
                          (BODY[1] - LID_C[1]) / dy if dy < 0 else (BODY[3] - LID_C[1]) / dy)]
        t = min(ts)
        d.line([P(LID_C[0] + dx * (LID_R + 3), LID_C[1] + dy * (LID_R + 3)), P(LID_C[0] + dx * t, LID_C[1] + dy * t)], fill=SEAM, width=S)
    # raised dots along both sides of the lid
    for a0 in (150, -25):
        for k in range(6):
            a = math.radians(a0 + k * 11)
            x, y = LID_C[0] + math.cos(a) * (LID_R + 9), LID_C[1] + math.sin(a) * (LID_R + 9)
            d.rectangle([P(x - 1.4, y - 1.4), P(x + 1.4, y + 1.4)], fill=(208, 206, 200))
            d.rectangle([P(x - 1.4, y - 1.4), P(x + 1.4, y - 0.6)], fill=(246, 245, 241))
    # tray well under the lid (seen while the lid is open)
    circ(d, LID_C, LID_R - 10, fill=(182, 180, 174))
    circ(d, LID_C, LID_R - 12, fill=(36, 36, 40))
    for r, c in ((70, (44, 44, 48)), (54, (50, 50, 54))): circ(d, LID_C, r, fill=c)
    d.rounded_rectangle([P(LID_C[0] - 6, LID_C[1] + 14), P(LID_C[0] + 6, LID_C[1] + 60)], radius=3 * S, fill=(26, 26, 28))   # laser rail
    for r, c in ((13, (150, 150, 146)), (9, (176, 176, 172)), (3, (96, 96, 92))): circ(d, LID_C, r, fill=c)
    # lid seat: a thin darker ring where the lid meets the shell, and the wedge tip on the shell
    circ(d, LID_C, LID_R + 1.5, outline=(186, 184, 178), width=int(1.2 * S))
    _wedge(d, LID_C[1] + LID_R - 8, LID_C[1] + LID_R + 10, 8, (124, 124, 120))
    _wedge(d, LID_C[1] + LID_R - 8, LID_C[1] + LID_R + 6, 5, (150, 150, 146))
    # POWER / OPEN
    font = ImageFont.truetype(TTF, 6 * S)
    for cx, label in ((36, "POWER"), (220, "OPEN")):
        c = (cx, 226)
        circ(d, c, 13, fill=(190, 188, 182))
        circ(d, c, 12, fill=GREY_BTN)
        circ(d, c, 10.5, outline=(200, 199, 193), width=S)
        tw = d.textlength(label, font=font)
        d.text((c[0] * S - tw / 2, (c[1] - 3.2) * S), label, font=font, fill=GREY_TEXT)
    return img.resize((256, 256), Image.LANCZOS)

def lid():
    img = Image.new("RGBA", (N, N), SHELL + (0,))                 # transparent texels keep the shell colour (bilinear fringe)
    face = _radial((N, N), (242, 241, 236), (222, 221, 215)).convert("RGBA")
    mask = Image.new("L", (N, N), 0); circ(ImageDraw.Draw(mask), LID_C, LID_R, fill=255)
    img.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(img)
    circ(d, LID_C, LID_R - 1.2, outline=(204, 202, 196), width=int(2.4 * S))   # rim, fully inside the alpha
    circ(d, LID_C, LID_R - 4.5, outline=(232, 231, 226), width=S)
    # logo lock-up in the upper third of the lid
    if os.path.exists(LOCKUP):
        lk = Image.open(LOCKUP).convert("RGBA")
        w = 40 * S; lk = lk.resize((w, int(lk.height * w / lk.width)), Image.LANCZOS)
        img.alpha_composite(lk, (128 * S - w // 2, 54 * S))
    # front wedge: recessed grey triangle, apex on the rim (the tip continues on the shell)
    wedge = Image.new("RGBA", (N, N), (0, 0, 0, 0)); wd = ImageDraw.Draw(wedge)
    _wedge(wd, LID_C[1] + 58, LID_C[1] + LID_R - 1, 15, (168, 168, 163, 255))
    wd.polygon([P(128, LID_C[1] + 58), P(128 + 15, LID_C[1] + 58), P(128, LID_C[1] + LID_R - 1)], fill=(184, 184, 179, 255))
    _wedge(wd, LID_C[1] + 60.5, LID_C[1] + LID_R - 4, 12.5, GREY_WEDGE + (255,))
    wedge.putalpha(Image.composite(wedge.getchannel("A"), Image.new("L", (N, N), 0), mask))
    img.alpha_composite(wedge)
    out = img.resize((256, 256), Image.LANCZOS)
    out.putalpha(out.getchannel("A").point(lambda v: 255 if v >= 128 else 0))   # 1-bit alpha: keep the rim crisp
    return out

def lid_inner():
    """Underside of the lid: plain shell, rim lip, disc clamp."""
    img = Image.new("RGBA", (N, N), SHELL + (0,))
    face = _radial((N, N), (214, 212, 206), (184, 182, 176)).convert("RGBA")
    mask = Image.new("L", (N, N), 0); circ(ImageDraw.Draw(mask), LID_C, LID_R, fill=255)
    img.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(img)
    circ(d, LID_C, LID_R, outline=(150, 148, 142), width=3 * S)
    for r, c in ((17, (160, 158, 152)), (13, (190, 188, 181)), (5, (120, 120, 116))): circ(d, LID_C, r, fill=c)
    out = img.resize((256, 256), Image.LANCZOS)
    out.putalpha(out.getchannel("A").point(lambda v: 255 if v >= 128 else 0))
    return out

def build(out_dir):
    from tools.cast_card import tex
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "body.tex"), "wb") as f: f.write(tex.encode(body(), "rgb565"))
    with open(os.path.join(out_dir, "lid.tex"), "wb") as f: f.write(tex.encode(lid(), "argb1555"))
    with open(os.path.join(out_dir, "lid_in.tex"), "wb") as f: f.write(tex.encode(lid_inner(), "argb1555"))

if __name__ == "__main__":
    import sys
    b, l = body(), lid()
    prev = b.convert("RGBA"); prev.alpha_composite(l)
    prev.save(sys.argv[1] if len(sys.argv) > 1 else "console_preview.png"); b.save("console_body.png")
