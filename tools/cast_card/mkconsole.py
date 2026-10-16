"""Top-view Dreamcast artwork for the launch animation (src/ui/launch_scene.c).
Two 256x256 textures in the same coordinate space so the lid sits on the body:
  body.tex  RGB565    console with the lid off: tray well, spindle, buttons, seams
  lid.tex   ARGB1555  the round lid: swirl, wordmark, front wedge (transparent outside)
Drawn at 4x and downsampled. Reference: top-down photo of a VA1 NTSC unit."""
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
TTF = os.path.join(ROOT, "assets", "fonts", "Dreamcast.ttf")
SWIRL = os.path.join(ROOT, "assets", "brand", "swirl.png")
S = 4                                   # supersample
N = 256 * S
LID_C, LID_R = (128 * S, 122 * S), 90 * S
SHELL, SHELL_DK, SEAM = (228, 226, 219), (196, 194, 187), (189, 187, 180)

def _grad(size, c0, c1, angle=45):
    """Linear gradient image c0 -> c1 along `angle` degrees."""
    w, h = size
    g = Image.linear_gradient("L").rotate(angle, expand=True).resize((w, h))
    a, b = Image.new("RGB", (w, h), c0), Image.new("RGB", (w, h), c1)
    return Image.composite(b, a, g)

def _wedge(d, y_top, y_apex, half, fill):
    d.polygon([(128 * S - half, y_top), (128 * S + half, y_top), (128 * S, y_apex)], fill=fill)

def body():
    img = Image.new("RGB", (N, N), (0, 0, 0))
    shell = _grad((N, N), (236, 234, 228), (214, 212, 205), 60)
    mask = Image.new("L", (N, N), 0); md = ImageDraw.Draw(mask)
    md.rounded_rectangle([10 * S, 10 * S, 246 * S, 246 * S], radius=26 * S, fill=255)
    img.paste(shell, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([10 * S, 10 * S, 246 * S, 246 * S], radius=26 * S, outline=SHELL_DK, width=2 * S)
    d.rectangle([12 * S, 238 * S, 244 * S, 246 * S], fill=(206, 204, 197))          # front lip
    # seams of the raised lid plateau
    for a, b in (((22, 22), (66, 64)), ((234, 22), (190, 64)), ((198, 176), (240, 226))):
        d.line([(a[0] * S, a[1] * S), (b[0] * S, b[1] * S)], fill=SEAM, width=S)
    # raised nubs along both sides of the lid
    for side in (-1, 1):
        for k in range(6):
            ang = math.radians((180 if side < 0 else 0) + (-37 + k * 15))
            x, y = LID_C[0] + math.cos(ang) * (LID_R + 13 * S), LID_C[1] + math.sin(ang) * (LID_R + 13 * S)
            d.rectangle([x - 1.6 * S, y - 1.6 * S, x + 1.6 * S, y + 1.6 * S], fill=(200, 198, 191))
    # tray well
    well = 84 * S
    d.ellipse([LID_C[0] - well - 2 * S, LID_C[1] - well - 2 * S, LID_C[0] + well + 2 * S, LID_C[1] + well + 2 * S], fill=(188, 186, 179))
    d.ellipse([LID_C[0] - well, LID_C[1] - well, LID_C[0] + well, LID_C[1] + well], fill=(38, 38, 42))
    for r, c in ((72 * S, (46, 46, 50)), (56 * S, (52, 52, 56))):
        d.ellipse([LID_C[0] - r, LID_C[1] - r, LID_C[0] + r, LID_C[1] + r], fill=c)
    d.rounded_rectangle([LID_C[0] - 6 * S, LID_C[1] + 14 * S, LID_C[0] + 6 * S, LID_C[1] + 60 * S], radius=3 * S, fill=(28, 28, 30))  # laser rail
    d.ellipse([LID_C[0] - 13 * S, LID_C[1] - 13 * S, LID_C[0] + 13 * S, LID_C[1] + 13 * S], fill=(150, 150, 146))
    d.ellipse([LID_C[0] - 9 * S, LID_C[1] - 9 * S, LID_C[0] + 9 * S, LID_C[1] + 9 * S], fill=(176, 176, 172))
    d.ellipse([LID_C[0] - 3 * S, LID_C[1] - 3 * S, LID_C[0] + 3 * S, LID_C[1] + 3 * S], fill=(96, 96, 92))
    # wedge apex below the lid
    _wedge(d, 206 * S, 229 * S, 10 * S, (176, 175, 168))
    # buttons
    font = ImageFont.truetype(TTF, 6 * S)
    for cx, label in ((46, "POWER"), (210, "OPEN")):
        c = (cx * S, 218 * S)
        d.ellipse([c[0] - 14 * S, c[1] - 14 * S, c[0] + 14 * S, c[1] + 14 * S], fill=(171, 169, 162))
        d.ellipse([c[0] - 12 * S, c[1] - 12 * S, c[0] + 12 * S, c[1] + 12 * S], fill=(206, 204, 197))
        tw = d.textlength(label, font=font)
        d.text((c[0] - tw / 2, c[1] - 3.5 * S), label, font=font, fill=(122, 120, 115))
    return img.resize((256, 256), Image.LANCZOS)

def lid():
    img = Image.new("RGBA", (N, N), SHELL + (0,))                 # transparent texels keep the shell colour (bilinear fringe)
    face = _grad((N, N), (244, 242, 236), (216, 214, 207), 50).convert("RGBA")
    mask = Image.new("L", (N, N), 0); md = ImageDraw.Draw(mask)
    md.ellipse([LID_C[0] - LID_R, LID_C[1] - LID_R, LID_C[0] + LID_R, LID_C[1] + LID_R], fill=255)
    img.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.ellipse([LID_C[0] - LID_R, LID_C[1] - LID_R, LID_C[0] + LID_R, LID_C[1] + LID_R], outline=SHELL_DK, width=int(1.5 * S))
    r2 = LID_R - 7 * S
    d.ellipse([LID_C[0] - r2, LID_C[1] - r2, LID_C[0] + r2, LID_C[1] + r2], outline=(222, 220, 213), width=S)
    # logo: swirl in a tombstone plate, wordmark under it
    d.rounded_rectangle([104 * S, 58 * S, 152 * S, 92 * S], radius=6 * S, outline=(206, 204, 197), width=S)
    sw = Image.open(SWIRL).convert("RGBA").resize((22 * S, 22 * S), Image.LANCZOS)
    img.paste(sw, (128 * S - 11 * S, 60 * S), sw)
    font = ImageFont.truetype(TTF, 9 * S)
    tw = d.textlength("Dreamcast", font=font)
    d.text((128 * S - tw / 2, 81 * S), "Dreamcast", font=font, fill=(96, 96, 92))
    # front wedge (inside the circle; the apex lives on the body)
    wedge = Image.new("RGBA", (N, N), (0, 0, 0, 0)); wd = ImageDraw.Draw(wedge)
    _wedge(wd, 186 * S, 229 * S, 20 * S, (186, 185, 178, 255))
    _wedge(wd, 190 * S, 222 * S, 15 * S, (204, 203, 196, 255))
    wedge.putalpha(Image.composite(wedge.getchannel("A"), Image.new("L", (N, N), 0), mask))
    img.alpha_composite(wedge)
    out = img.resize((256, 256), Image.LANCZOS)
    a = out.getchannel("A").point(lambda v: 255 if v >= 128 else 0)   # 1-bit alpha: keep the rim crisp
    out.putalpha(a)
    return out

def lid_inner():
    """Underside of the lid, seen once it swings past edge-on: plain shell, rim lip, disc clamp."""
    img = Image.new("RGBA", (N, N), SHELL + (0,))
    face = _grad((N, N), (206, 204, 197), (178, 176, 169), 230).convert("RGBA")
    mask = Image.new("L", (N, N), 0); md = ImageDraw.Draw(mask)
    md.ellipse([LID_C[0] - LID_R, LID_C[1] - LID_R, LID_C[0] + LID_R, LID_C[1] + LID_R], fill=255)
    img.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.ellipse([LID_C[0] - LID_R, LID_C[1] - LID_R, LID_C[0] + LID_R, LID_C[1] + LID_R], outline=(150, 148, 142), width=3 * S)
    for r, c in ((17 * S, (160, 158, 152)), (13 * S, (190, 188, 181)), (5 * S, (120, 120, 116))):
        d.ellipse([LID_C[0] - r, LID_C[1] - r, LID_C[0] + r, LID_C[1] + r], fill=c)
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
