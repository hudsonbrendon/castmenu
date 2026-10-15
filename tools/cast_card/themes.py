"""Built-in theme definitions. Each theme = theme.ini + generated bg.png (640x480)."""
import math
from PIL import Image, ImageDraw, ImageFilter

THEMES = {
 "ocean":        dict(name="Ocean", bg="#07253F", panel="#CC0B3558", panel_alt="#CC12456E", accent="#2EC8FF", accent2="#7EF2E0", glow="#8020B8FF", accent_dim="#1A6E99", text="#E6F4FF", text_dim="#8FBAD6", text_hi="#FFFFFF", sel_bg="#2EC8FF", sel_text="#04203A", badge_j="#FF5A6E", badge_u="#3B9BFF", badge_e="#2ED47A", badge_gd="#FFB03B", badge_cd="#6C8AA6", grad=((5,30,56),(10,70,120)), waves=(46,200,255), orbs=[(500,120,200,(46,200,255)),(120,420,220,(30,120,200))]),
 "night":        dict(name="Night", bg="#0A0A14", panel="#CC14142A", panel_alt="#CC1E1E3C", accent="#8B7CFF", accent2="#FF7CC8", glow="#808B7CFF", accent_dim="#3F3A80", text="#ECEAFF", text_dim="#9A96C8", text_hi="#FFFFFF", sel_bg="#8B7CFF", sel_text="#0A0A14", badge_cd="#5A5A7A", grad=((10,10,20),(28,22,60)), stars=True, orbs=[(560,80,180,(139,124,255)),(100,400,200,(255,124,200))]),
 "classic":      dict(name="Classic", bg="#FFFFFF", panel="#E6FFFFFF", panel_alt="#E6EEF1F5", accent="#F26A1B", accent2="#1B8CF2", glow="#40F26A1B", accent_dim="#F9C7A8", text="#222222", text_dim="#747C8A", text_hi="#000000", sel_bg="#F26A1B", sel_text="#FFFFFF", badge_gd="#F26A1B", badge_cd="#8A90A0", shadow="#28000000", vignette=25, grad=((255,255,255),(226,232,240)), swirl=(242,106,27), orbs=[(540,90,180,(242,106,27))]),
 "cast-dark":  dict(name="Cast Dark", bg="#0E0F14", panel="#CC181A22", panel_alt="#CC222533", accent="#5B8CFF", accent2="#A06BFF", glow="#805B8CFF", accent_dim="#2E4A8A", text="#F2F3F5", text_dim="#8B90A0", text_hi="#FFFFFF", sel_bg="#5B8CFF", sel_text="#0E0F14", grad=((14,15,20),(30,34,60)), orbs=[(120,380,220,(91,140,255)),(560,90,180,(160,90,255))]),
 "cast-light": dict(name="Cast Light", bg="#F4F5F9", panel="#E6FFFFFF", panel_alt="#E6EEF0F6", accent="#3366FF", accent2="#FF8C5A", glow="#403366FF", accent_dim="#C7D3FF", text="#1C1E26", shadow="#30000000", vignette=40, text_dim="#6B7080", text_hi="#000000", sel_bg="#3366FF", sel_text="#FFFFFF", badge_cd="#8A90A0", grad=((244,245,249),(214,222,245)), orbs=[(80,420,200,(51,102,255)),(600,60,160,(255,140,90))], bg_dim=0),
 "dreamcast":    dict(name="Dreamcast Classic", bg="#FFFFFF", panel="#E6FFFFFF", panel_alt="#E6F2F2F2", accent="#F26A1B", accent2="#1B8CF2", glow="#40F26A1B", accent_dim="#F9C7A8", text="#222222", shadow="#30000000", vignette=30, text_dim="#777777", text_hi="#000000", sel_bg="#F26A1B", sel_text="#FFFFFF", badge_gd="#F26A1B", grad=((255,255,255),(240,240,240)), swirl=(242,106,27)),
 "synthwave":    dict(name="Synthwave", bg="#12071F", panel="#CC1C0B33", panel_alt="#CC2A1250", accent="#FF2EAB", accent2="#2EE6FF", glow="#80FF2EAB", accent_dim="#7A1A5A", text="#FDE7FF", text_dim="#B38BC9", text_hi="#FFFFFF", sel_bg="#FF2EAB", sel_text="#12071F", badge_j="#FF2EAB", badge_u="#2EE6FF", badge_e="#FFD400", badge_gd="#FF8A00", grad=((18,7,31),(90,10,80)), sun=True),
 "terminal":     dict(name="Terminal", bg="#000000", panel="#CC0A0F0A", panel_alt="#CC102010", accent="#39FF14", accent2="#39FF14", glow="#4039FF14", accent_dim="#145A0E", text="#C8FFC0", orbs=0, vignette=140, text_dim="#5FA85A", text_hi="#E8FFE0", sel_bg="#39FF14", sel_text="#000000", badge_j="#39FF14", badge_u="#39FF14", badge_e="#39FF14", badge_gd="#39FF14", badge_cd="#2A7A20", grad=((0,0,0),(4,16,4)), scanlines=True, panel_radius=2),
 "sega-blue":    dict(name="Sega Blue", bg="#0B1E4A", panel="#CC0F2A66", panel_alt="#CC173C8C", accent="#17A8FF", accent2="#5BE0FF", glow="#8017A8FF", accent_dim="#0C5A8A", text="#E8F2FF", text_dim="#8FB3DE", text_hi="#FFFFFF", sel_bg="#17A8FF", sel_text="#0B1E4A", grad=((11,30,74),(23,60,140)), orbs=[(320,470,320,(23,168,255))]),
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
        d = ImageDraw.Draw(img); col = spec["swirl"]
        cx, cy = 520, 120
        for i in range(0, 900):
            a = i / 60.0; rr = 6 + i * 0.12
            x, y = cx + rr * math.cos(a), cy + rr * math.sin(a)
            d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=col)
        img = img.filter(ImageFilter.GaussianBlur(0.6))
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
