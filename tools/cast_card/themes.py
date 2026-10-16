"""Built-in theme definitions. Each theme = theme.ini + generated bg.png (640x480)."""
import math
from PIL import Image, ImageDraw, ImageFilter

# Dreamcast palette: the logo red, the four controller buttons, console shell and logotype grey.
DC = dict(red="#CF3311", orange="#CF3311", btn_red="#E23D3D", blue="#2D6BD1", yellow="#F2C229", green="#3DB55A", shell="#E9E9E5", grey="#66696D", ink="#26272A")
# Badges (region, GD/CD, VGA, disc) follow the console: shell grey on light themes, slate on dark ones.
# badge_j / badge_e double as the status colours (error / ok) and stay red / green.
BUTTONS = dict(badge_j=DC["btn_red"], badge_u="#D6D6D2", badge_e=DC["green"], badge_gd="#D6D6D2", badge_cd="#D6D6D2")
BUTTONS_DARK = dict(badge_j=DC["btn_red"], badge_u="#4C4C52", badge_e=DC["green"], badge_gd="#4C4C52", badge_cd="#4C4C52")

THEMES = {
 "dreamcast":      dict(name="Dreamcast", bg="#F2F2EF", panel="#F0FFFFFF", panel_alt="#F0E4E4E0", accent=DC["red"], accent2=DC["grey"], glow="#40CF3311", accent_dim="#F0C4B8", text=DC["ink"], text_dim=DC["grey"], text_hi="#141416", sel_bg=DC["red"], sel_text="#FFFFFF", shadow="#30000000", vignette=16, grad=((245,245,242),(216,216,212)), swirl=(207,51,17), orbs=[(540,90,170,(207,51,17)),(110,420,160,(102,105,109))], **BUTTONS),
 "dreamcast-dark": dict(name="Dreamcast Dark", bg="#1C1C1E", panel="#CC2A2A2E", panel_alt="#CC38383E", accent=DC["red"], accent2=DC["grey"], glow="#70CF3311", accent_dim="#6E2A18", text="#ECECEA", text_dim="#A8AAAD", text_hi="#FFFFFF", sel_bg=DC["red"], sel_text="#FFFFFF", grad=((28,28,30),(50,50,56)), swirl=(207,51,17), orbs=[(540,90,180,(207,51,17)),(120,400,200,(102,105,109))], **BUTTONS_DARK),
 "controller":     dict(name="Controller", bg="#2B2B30", panel="#CC35353B", panel_alt="#CC45454C", accent=DC["red"], accent2=DC["green"], glow="#60CF3311", accent_dim="#6E2A18", text="#F2F2F0", text_dim="#B0B0B4", text_hi="#FFFFFF", sel_bg=DC["blue"], sel_text="#FFFFFF", grad=((43,43,48),(70,70,78)), buttons=True, orbs=[(120,120,150,(226,61,61)),(520,120,150,(45,107,209)),(120,380,150,(242,194,41)),(520,380,150,(61,181,90))], **BUTTONS_DARK),
 "ocean":          dict(name="Ocean", bg="#07253F", panel="#CC0B3558", panel_alt="#CC12456E", accent="#2EC8FF", accent2=DC["red"], glow="#8020B8FF", accent_dim="#1A6E99", text="#E6F4FF", text_dim="#8FBAD6", text_hi="#FFFFFF", sel_bg="#2EC8FF", sel_text="#04203A", grad=((5,30,56),(10,70,120)), waves=(46,200,255), orbs=[(500,120,200,(46,200,255)),(120,420,220,(30,120,200))], **BUTTONS_DARK),
 "night":          dict(name="Night", bg="#0A0A14", panel="#CC14142A", panel_alt="#CC1E1E3C", accent="#8B7CFF", accent2=DC["red"], glow="#808B7CFF", accent_dim="#3F3A80", text="#ECEAFF", text_dim="#9A96C8", text_hi="#FFFFFF", sel_bg="#8B7CFF", sel_text="#0A0A14", grad=((10,10,20),(28,22,60)), stars=True, orbs=[(560,80,180,(139,124,255)),(100,400,200,(207,51,17))], **BUTTONS_DARK),
 "synthwave":      dict(name="Synthwave", bg="#12071F", panel="#CC1C0B33", panel_alt="#CC2A1250", accent="#FF2EAB", accent2="#2EE6FF", glow="#80FF2EAB", accent_dim="#7A1A5A", text="#FDE7FF", text_dim="#B38BC9", text_hi="#FFFFFF", sel_bg="#FF2EAB", sel_text="#12071F", grad=((18,7,31),(90,10,80)), sun=True, **BUTTONS_DARK),
 "terminal":       dict(name="Terminal", bg="#000000", panel="#CC0A0F0A", panel_alt="#CC102010", accent="#39FF14", accent2="#39FF14", glow="#4039FF14", accent_dim="#145A0E", text="#C8FFC0", orbs=0, vignette=140, text_dim="#5FA85A", text_hi="#E8FFE0", sel_bg="#39FF14", sel_text="#000000", grad=((0,0,0),(4,16,4)), scanlines=True, panel_radius=2, **BUTTONS_DARK),
 "sega-blue":      dict(name="Sega Blue", bg="#0B1E4A", panel="#CC0F2A66", panel_alt="#CC173C8C", accent="#17A8FF", accent2=DC["red"], glow="#8017A8FF", accent_dim="#0C5A8A", text="#E8F2FF", text_dim="#8FB3DE", text_hi="#FFFFFF", sel_bg="#17A8FF", sel_text="#0B1E4A", grad=((11,30,74),(23,60,140)), orbs=[(320,470,320,(23,168,255))], **BUTTONS_DARK),
}

def render_bg(spec, size=(640, 480)):
    w, h = size
    img = Image.new("RGB", size, spec["grad"][0])
    d = ImageDraw.Draw(img)
    (r0, g0, b0), (r1, g1, b1) = spec["grad"]
    for y in range(h):
        t = y / (h - 1)
        d.line([(0, y), (w, y)], fill=(int(r0 + (r1 - r0) * t), int(g0 + (g1 - g0) * t), int(b0 + (b1 - b0) * t)))
    if spec.get("orbs"):
        glow = Image.new("RGB", size, (0, 0, 0)); gd = ImageDraw.Draw(glow)
        for (cx, cy, rad, col) in spec["orbs"]:
            gd.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=col)
        glow = glow.filter(ImageFilter.GaussianBlur(70))
        img = Image.blend(img, Image.composite(glow, img, glow.convert("L").point(lambda v: min(255, v * 2))), 0.45)
    if spec.get("swirl"):
        # big faint Dreamcast-style swirl watermark on the right
        layer = Image.new("RGBA", size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer); col = spec["swirl"]
        cx, cy = 530, 130
        pts = []
        for i in range(0, 1150):
            a = i / 95.0; rr = 3 + a * 9
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        for k in range(len(pts) - 1):
            d.line([pts[k], pts[k + 1]], fill=col + (30,), width=int(6 + k / 110))
        img = Image.alpha_composite(img.convert("RGBA"), layer.filter(ImageFilter.GaussianBlur(1.2))).convert("RGB")
    if spec.get("buttons"):
        d = ImageDraw.Draw(img, "RGBA")
        for (x, y, c) in ((500, 120, (226, 61, 61)), (560, 70, (45, 107, 209)), (440, 70, (242, 194, 41)), (500, 20, (61, 181, 90))):
            d.ellipse([x - 26, y - 26, x + 26, y + 26], fill=c + (60,))
    if spec.get("sun"):
        d = ImageDraw.Draw(img)
        for i in range(120, 0, -1):
            t = i / 120
            d.ellipse([320 - i, 150 - i, 320 + i, 150 + i], fill=(255, int(60 + 150 * (1 - t)), int(120 * (1 - t))))
        for yy in range(150, 270, 10): d.rectangle([200, yy, 440, yy + 3], fill=spec["grad"][1])
        for x in range(-640, 1280, 80): d.line([(320, 270), (x, 480)], fill=(255, 46, 171))
        for yy in (300, 340, 390, 450): d.line([(0, yy), (640, yy)], fill=(255, 46, 171))
    if spec.get("waves"):
        # layered sine waves with soft highlights, like light caustics
        d = ImageDraw.Draw(img, "RGBA"); r, g, b = spec["waves"]
        for layer in range(5):
            amp = 14 + layer * 7; freq = 0.006 + layer * 0.0025; ybase = 150 + layer * 70; ph = layer * 1.9
            pts = [(x, ybase + amp * math.sin(x * freq + ph) + amp * 0.4 * math.sin(x * freq * 2.1 + ph * 2)) for x in range(0, w + 1, 4)]
            d.line(pts, fill=(r, g, b, 22 + layer * 5), width=34 - layer * 4)
            d.line([(x, y - 4) for x, y in pts], fill=(255, 255, 255, 14), width=3)
        img = img.filter(ImageFilter.GaussianBlur(9))
    if spec.get("stars"):
        import random
        rnd = random.Random(7); d = ImageDraw.Draw(img, "RGBA")
        for _ in range(140):
            x, y, s = rnd.randrange(w), rnd.randrange(h), rnd.choice([1, 1, 1, 2])
            d.ellipse([x, y, x + s, y + s], fill=(255, 255, 255, rnd.randrange(60, 200)))
    if spec.get("scanlines"):
        d = ImageDraw.Draw(img)
        for yy in range(0, h, 3): d.line([(0, yy), (w, yy)], fill=(0, 8, 0))
    return img

def write_ini(spec, path):
    keys = ["bg","panel","panel_alt","accent","accent2","accent_dim","glow","text","text_dim","text_hi","sel_bg","sel_text","badge_j","badge_u","badge_e","badge_gd","badge_cd","shadow","layout","panel_radius","bg_dim","cover_frame","font_scale","orbs","vignette"]
    with open(path, "w") as f:
        f.write("[theme]\n")
        f.write(f"name={spec['name']}\nauthor=Cast Menu\n")
        for k in keys:
            if k in spec: f.write(f"{k}={spec[k]}\n")
