"""Cast Menu brand assets from the Dreamcast logo (assets/brand/dreamcast_logo.svg,
Wikimedia Commons, PD-textlogo; the swirl and the Dreamcast name are SEGA trademarks).
Needs rsvg-convert (librsvg). Writes to assets/brand:
  dreamcast_lockup.png   swirl + "Dreamcast" as in the SVG (used on the console lid)
  swirl.png              the swirl alone, transparent
  logo_vertical.png      swirl over "Cast Menu", transparent
  logo_sticker.png       the same with a white die-cut border and a soft shadow (README / social)
  wordmark_1086.png      horizontal lock-up for the README header
  icon*.png / icon_transparent.png   square icons (sticker swirl)
  social_preview.png     1280x640 GitHub social card
Run: python3 -m tools.cast_card.mkbrand"""
import os, subprocess, tempfile
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BRAND = os.path.join(ROOT, "assets", "brand")
SVG = os.path.join(BRAND, "dreamcast_logo.svg")
TTF = os.path.join(ROOT, "assets", "fonts", "Dreamcast.ttf")
INK = (20, 20, 22)

def render_svg(width=2400):
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f: out = f.name
    subprocess.run(["rsvg-convert", "-w", str(width), SVG, "-o", out], check=True)
    img = Image.open(out).convert("RGBA"); os.remove(out)
    return img

def split(img):
    """swirl = the red pixels, wordmark = the dark ones."""
    px = img.load(); w, h = img.size
    sw, wm = Image.new("RGBA", img.size, (0, 0, 0, 0)), Image.new("RGBA", img.size, (0, 0, 0, 0))
    sp, wp = sw.load(), wm.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0: continue
            (sp if r > 120 and r > g + 40 else wp)[x, y] = (r, g, b, a)
    return sw.crop(sw.getbbox()), wm.crop(wm.getbbox())

def text_img(text, height):
    font = ImageFont.truetype(TTF, height, layout_engine=ImageFont.Layout.BASIC)
    l, t, r, b = font.getbbox(text)
    img = Image.new("RGBA", (r - l + 8, b - t + 8), (0, 0, 0, 0))
    ImageDraw.Draw(img).text((4 - l, 4 - t), text, font=font, fill=INK + (255,))
    return img.crop(img.getbbox())

def lockup_vertical(swirl, text, gap_ratio=0.10):
    """Swirl centred over the wordmark; the wordmark is a bit wider than the swirl, as on the console logo."""
    tw = int(swirl.width * 1.28); text = text.resize((tw, int(text.height * tw / text.width)), Image.LANCZOS)
    gap = int(swirl.height * gap_ratio)
    img = Image.new("RGBA", (tw, swirl.height + gap + text.height), (0, 0, 0, 0))
    img.alpha_composite(swirl, ((tw - swirl.width) // 2, 0)); img.alpha_composite(text, (0, swirl.height + gap))
    return img

def sticker(img, border, shadow=True):
    """White die-cut outline around the alpha, like an enamel pin."""
    pad = border * 3
    canvas = Image.new("RGBA", (img.width + 2 * pad, img.height + 2 * pad), (255, 255, 255, 0))
    a = Image.new("L", canvas.size, 0); a.paste(img.getchannel("A"), (pad, pad))
    a = a.filter(ImageFilter.MaxFilter(2 * (border // 2) + 1)).filter(ImageFilter.MaxFilter(2 * (border - border // 2) + 1))
    a = a.filter(ImageFilter.GaussianBlur(border * 0.25)).point(lambda v: 255 if v > 110 else 0)
    if shadow:
        sh = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); sh.putalpha(a.point(lambda v: v * 0.35))
        sh = sh.filter(ImageFilter.GaussianBlur(border * 0.9)); shifted = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); shifted.alpha_composite(sh, (0, border // 2))
        canvas.alpha_composite(shifted)
    white = Image.new("RGBA", canvas.size, (255, 255, 255, 0)); white.putalpha(a)
    canvas.alpha_composite(white); canvas.alpha_composite(img, (pad, pad))
    return canvas.crop(canvas.getbbox())

def build():
    swirl, _ = split(render_svg())
    swirl.save(os.path.join(BRAND, "swirl.png"))
    render_svg(1200).crop(render_svg(1200).getbbox()).save(os.path.join(BRAND, "dreamcast_lockup.png"))
    text = text_img("Cast Menu", 400)
    vert = lockup_vertical(swirl, text)
    vert.save(os.path.join(BRAND, "logo_vertical.png"))
    st = sticker(vert, 36); st.save(os.path.join(BRAND, "logo_sticker.png"))
    # horizontal lock-up for the README: sticker swirl + wordmark
    sw_small = sticker(swirl, 30, shadow=False); h = 320
    sw_small = sw_small.resize((int(sw_small.width * h / sw_small.height), h), Image.LANCZOS)
    tx = text.resize((int(text.width * (h * 0.62) / text.height), int(h * 0.62)), Image.LANCZOS)
    pad = 40
    wm = Image.new("RGBA", (sw_small.width + 70 + tx.width + 2 * pad, h + 2 * pad), (255, 255, 255, 0))
    wm.alpha_composite(sw_small, (pad, pad)); wm.alpha_composite(tx, (pad + sw_small.width + 70, pad + (h - tx.height) // 2))
    wm.resize((1086, int(wm.height * 1086 / wm.width)), Image.LANCZOS).save(os.path.join(BRAND, "wordmark_1086.png"))
    wm.save(os.path.join(BRAND, "wordmark.png"))
    # icons: sticker swirl on transparent / on shell grey
    ic = sticker(swirl, 44, shadow=False); side = max(ic.size) + 60
    sq = Image.new("RGBA", (side, side), (255, 255, 255, 0)); sq.alpha_composite(ic, ((side - ic.width) // 2, (side - ic.height) // 2))
    sq.resize((512, 512), Image.LANCZOS).save(os.path.join(BRAND, "icon_transparent.png"))
    for s in (512, 256, 128, 64):
        bg = Image.new("RGBA", (side, side), (26, 27, 30, 255)); bg.alpha_composite(sq)
        bg.resize((s, s), Image.LANCZOS).save(os.path.join(BRAND, f"icon_{s}.png"))
    Image.open(os.path.join(BRAND, "icon_512.png")).save(os.path.join(BRAND, "icon.png"))
    # social preview 1280x640
    sp = Image.new("RGBA", (1280, 640), (26, 27, 30, 255))
    s2 = st.resize((int(st.width * 520 / st.height), 520), Image.LANCZOS)
    sp.alpha_composite(s2, ((1280 - s2.width) // 2, 60)); sp.convert("RGB").save(os.path.join(BRAND, "social_preview.png"))
    print("brand ok")

if __name__ == "__main__":
    build()
