"""TTF -> bitmap font atlas (.tex ARGB4444) + metrics (.fnt) for src/ui/font.c.
Covers Latin-1 (chars 32..255) so pt-BR accents render."""
import struct
from PIL import Image, ImageDraw, ImageFont
from . import tex

def build(ttf, px, out_base, weight="Regular", atlas=(512, 256), stroke=0):
    """stroke: extra outline width in px (fake bold for single-weight fonts)."""
    font = ImageFont.truetype(ttf, px, layout_engine=ImageFont.Layout.BASIC)
    try: font.set_variation_by_name(weight)
    except Exception: pass
    pad = 1
    atlas_img = Image.new("RGBA", atlas, (255, 255, 255, 0))
    draw = ImageDraw.Draw(atlas_img)
    ascent, descent = font.getmetrics()
    line_h = ascent + descent
    x = y = pad; row_h = 0
    recs = []
    for code in range(32, 256):
        ch = chr(code)
        bbox = font.getbbox(ch, anchor="ls", stroke_width=stroke)  # baseline-left
        adv = max(1, round(font.getlength(ch)) + stroke)
        if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            recs.append((0, 0, 0, 0, 0, 0, adv)); continue
        gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        if x + gw + pad > atlas[0]: x = pad; y += row_h + pad; row_h = 0
        if y + gh + pad > atlas[1]: raise SystemExit(f"atlas too small for {px}px")
        draw.text((x - bbox[0], y - bbox[1]), ch, font=font, fill=(255, 255, 255, 255), anchor="ls", stroke_width=stroke, stroke_fill=(255, 255, 255, 255))
        recs.append((x, y, gw, gh, bbox[0], ascent + bbox[1], adv))
        x += gw + pad; row_h = max(row_h, gh)
    with open(out_base + ".tex", "wb") as f: f.write(tex.encode(atlas_img, "argb4444"))
    with open(out_base + ".fnt", "wb") as f:
        f.write(b"NFNT" + struct.pack("<BBH", line_h, ascent, 0))
        for (gx, gy, gw, gh, xo, yo, adv) in recs:
            f.write(struct.pack("<HHBBbbBB", gx, gy, min(gw, 255), min(gh, 255), max(-128, min(127, xo)), max(-128, min(127, yo)), min(adv, 255), 0))
    return line_h
