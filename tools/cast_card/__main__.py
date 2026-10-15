"""castmenu-card: prepare a GDEMU SD card for the Cast Menu menu.

  python3 -m tools.cast_card scan  <sd_root>                 list what is on the card
  python3 -m tools.cast_card build <sd_root> [--out DIR]     write LIST.INI/CASTMENU.INI, covers, themes, 01/ menu image
  python3 -m tools.cast_card add   <sd_root> <image>...      copy images into the next free folders (then build)
  python3 -m tools.cast_card sort  <sd_root>                 renumber folders alphabetically (02..)

Folders follow the GDEMU rule: 01 = menu, 02..999 = one image per folder.

Per-game extras next to the image: name.txt, cover.png, genre.txt, year.txt, players.txt,
publisher.txt, category.txt (or platform.txt), synopsis.txt / synopsis.<pt|en|es|de|fr|it>.txt.
Card root extras: avatars/*.png (profile pictures), server.txt (host:port of the Cast Chat server).
"""
import argparse, os, re, shutil, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.cast_card import disc, ipbin, gdi, tex, themes, platforms
LANGS = ["pt", "en", "es", "de", "fr", "it"]
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def folder_num(name):
    return int(name) if re.fullmatch(r"\d{2,3}", name) else None

def scan(sd):
    items = []
    for name in sorted(os.listdir(sd)):
        n = folder_num(name)
        if n is None or n < 2: continue
        folder = os.path.join(sd, name)
        img = disc.find_image(folder)
        if not img: items.append(dict(slot=n, folder=folder, error="no image")); continue
        try:
            info = disc.read_image(img)
        except Exception as e:
            # unreadable image (odd CDI layout): still list it, from name.txt/serial.txt, so GDEMU can boot it
            nt, st = os.path.join(folder, "name.txt"), os.path.join(folder, "serial.txt")
            if os.path.exists(nt):
                title = open(nt, encoding="utf-8", errors="replace").read().strip() or os.path.basename(folder)
                serial = open(st).read().strip() if os.path.exists(st) else ""
                items.append(dict(slot=n, folder=folder, image=img, ip=ipbin.fallback(title, serial), cover=None, name=title, warning=str(e)))
                print(f"  !! {os.path.basename(folder)}: {e} (listed from name.txt)")
            else:
                items.append(dict(slot=n, folder=folder, error=str(e)))
            continue
        title = info.ip.name
        nt = os.path.join(folder, "name.txt")
        if os.path.exists(nt):
            t = open(nt, encoding="utf-8", errors="replace").read().strip()
            if t: title = t
        items.append(dict(slot=n, folder=folder, image=img, ip=info.ip, cover=info.cover, name=title))
    return items

def list_ini(items, menu_name="CASTMENU", server=None):
    gd, nb = ["[GDMENU]", "01.name=%s" % menu_name, "01.disc=1/1", "01.vga=1", "01.region=JUE", "01.version=V1.000", "01.date=20261015", ""], ["[CASTMENU]"]
    if server: nb.append(f"server={server}")
    for it in items:
        if "error" in it: continue
        ip, s = it["ip"], "%02d" % it["slot"] if it["slot"] < 100 else "%03d" % it["slot"]
        gd += [f"{s}.name={it['name']}", f"{s}.disc={ip.disc}", f"{s}.vga={1 if ip.vga else 0}", f"{s}.region={ip.region}", f"{s}.version={ip.version}", f"{s}.date={ip.date}", ""]
        nb += [f"{s}.product={ip.product.replace(' ', '').replace('-', '')}", f"{s}.gd={1 if ip.is_gd else 0}", f"{s}.cover={1 if it.get('cover') else 0}"]
        files = {}
        for k in ("genre", "year", "players", "publisher", "category", "platform"):
            p = os.path.join(it["folder"], k + ".txt")
            if os.path.exists(p): files[k + ".txt"] = open(p, encoding="utf-8", errors="replace").read().strip()
        for k in ("genre", "year", "players", "publisher"):
            if files.get(k + ".txt"): nb.append(f"{s}.{k}={files[k + '.txt']}")
        cat = platforms.category_for(files, it["name"])
        if cat: nb.append(f"{s}.category={cat}")
        nb.append("")
    return "\n".join(gd) + "\n", "\n".join(nb) + "\n"

def write_meta(items):
    for it in items:
        if "error" in it or "warning" in it: continue
        with open(os.path.join(it["folder"], "name.txt"), "w", encoding="utf-8") as f: f.write(it["name"])
        with open(os.path.join(it["folder"], "serial.txt"), "w") as f: f.write(it["ip"].product)

def write_synopses(items, data_dir):
    """synopsis.<lang>.txt -> info/<lang>/NNN.txt ; synopsis.txt -> info/any/NNN.txt"""
    for it in items:
        if "error" in it: continue
        for lang in LANGS + ["any"]:
            src = os.path.join(it["folder"], "synopsis.txt" if lang == "any" else f"synopsis.{lang}.txt")
            if not os.path.exists(src): continue
            d = os.path.join(data_dir, "info", lang); os.makedirs(d, exist_ok=True)
            txt = open(src, encoding="utf-8", errors="replace").read().strip()
            open(os.path.join(d, "%03d.txt" % it["slot"]), "w", encoding="latin-1", errors="replace").write(txt[:2300])

def write_avatars(sd, data_dir):
    src = os.path.join(sd, "avatars")
    if not os.path.isdir(src): return
    d = os.path.join(data_dir, "avatars"); os.makedirs(d, exist_ok=True)
    for f in sorted(os.listdir(src)):
        if not f.lower().endswith((".png", ".jpg", ".jpeg")): continue
        name = re.sub(r"[^a-z0-9_-]", "", os.path.splitext(f)[0].lower())[:15]
        img = Image.open(os.path.join(src, f)).convert("RGB").resize((64, 64), Image.LANCZOS)
        open(os.path.join(d, name + ".tex"), "wb").write(tex.encode(img, "rgb565"))

def build_data(items, data_dir, menu_bin, themes_dir, custom_covers=None, sd=None, server=None):
    os.makedirs(os.path.join(data_dir, "covers"), exist_ok=True)
    shutil.copy(menu_bin, os.path.join(data_dir, "1ST_READ.BIN"))
    if sd and not server:
        st = os.path.join(sd, "server.txt")
        if os.path.exists(st): server = open(st).read().strip()
    gd, nb = list_ini(items, server=server)
    write_synopses(items, data_dir)
    if sd: write_avatars(sd, data_dir)
    open(os.path.join(data_dir, "LIST.INI"), "w", encoding="latin-1", errors="replace").write(gd)
    open(os.path.join(data_dir, "CASTMENU.INI"), "w", encoding="latin-1", errors="replace").write(nb)
    for it in items:
        if "error" in it: continue
        dst = os.path.join(data_dir, "covers", "%03d" % it["slot"])
        png = None
        for ext in ("png", "jpg", "jpeg"):
            p = os.path.join(it["folder"], "cover." + ext)
            if os.path.exists(p): png = p
        if custom_covers:
            p = os.path.join(custom_covers, it["ip"].product.replace(" ", "") + ".png")
            if os.path.exists(p): png = p
        if png:
            img = Image.open(png).convert("RGB").resize((128, 128), Image.LANCZOS)
            open(dst + ".tex", "wb").write(tex.encode(img, "rgb565")); it["cover"] = True
        elif it.get("cover"):
            open(dst + ".pvr", "wb").write(it["cover"])
    # themes: convert bg.png -> bg.tex (512x512) and copy ini
    for tid in sorted(os.listdir(themes_dir)):
        src = os.path.join(themes_dir, tid)
        if not os.path.isdir(src) or not os.path.exists(os.path.join(src, "theme.ini")): continue
        dst = os.path.join(data_dir, "themes", tid); os.makedirs(dst, exist_ok=True)
        shutil.copy(os.path.join(src, "theme.ini"), dst)
        for name in ("bg", "logo"):
            for ext in ("png", "jpg"):
                p = os.path.join(src, f"{name}.{ext}")
                if os.path.exists(p):
                    img = Image.open(p)
                    if name == "bg": img = img.convert("RGB").resize((512, 512), Image.LANCZOS)
                    open(os.path.join(dst, f"{name}.tex"), "wb").write(tex.encode(img, "rgb565" if name == "bg" else "argb4444"))

def cmd_build(a):
    items = scan(a.sd)
    bad = [i for i in items if "error" in i]
    for i in bad: print(f"  !! {os.path.basename(i['folder'])}: {i['error']}")
    good = [i for i in items if "error" not in i]
    print(f"{len(good)} jogos encontrados")
    write_meta(good)
    menu_bin = a.menu_bin or os.path.join(ROOT, "1ST_READ.BIN")
    if not os.path.exists(menu_bin): sys.exit(f"menu binary not found: {menu_bin} (run ./build.sh first)")
    out = a.out or os.path.join(a.sd, "01")
    with tempfile.TemporaryDirectory() as tmp:
        data = os.path.join(tmp, "data"); os.makedirs(data)
        build_data(items, data, menu_bin, a.themes or os.path.join(ROOT, "themes"), a.covers, sd=a.sd, server=getattr(a, "server", None))
        ip_tmpl = open(a.ip_template or os.path.join(ROOT, "assets", "IP.BIN"), "rb").read()
        ip = ipbin.patch(ip_tmpl, name="CASTMENU", product="CASTMENU", version="V1.000", date="20261015", company="CASTMENU")
        ipp = os.path.join(tmp, "IP.BIN"); open(ipp, "wb").write(ip)
        if os.path.isdir(out):
            for f in os.listdir(out):
                try: os.remove(os.path.join(out, f))
                except FileNotFoundError: pass   # macOS drops ._AppleDouble files together with their owner
        gdi.build(out, data, ipp)
    cfg = os.path.join(a.sd, "GDEMU.ini")
    if not os.path.exists(cfg):
        open(cfg, "w").write("open_time = 150\ndetect_time = 150\nreset_goto = 1\n")
    print(f"menu escrito em {out}")

def cmd_scan(a):
    for it in scan(a.sd):
        if "error" in it: print(f"{it['slot']:03d}  !! {it['error']}"); continue
        ip = it["ip"]
        print(f"{it['slot']:03d}  {it['name'][:40]:40s} {ip.region:3s} {'GD' if ip.is_gd else 'CD'} {ip.disc:3s} {'VGA' if ip.vga else '   '} {ip.product:10s} {ip.date} {'cover' if it['cover'] else ''}")

def cmd_add(a):
    used = {folder_num(n) for n in os.listdir(a.sd) if folder_num(n)}
    nxt = 2
    for img in a.images:
        while nxt in used: nxt += 1
        dst = os.path.join(a.sd, "%02d" % nxt if nxt < 100 else "%03d" % nxt); os.makedirs(dst)
        srcdir = os.path.dirname(os.path.abspath(img))
        files = [img]
        if img.lower().endswith(".gdi"):
            files += [os.path.join(srcdir, t["file"]) for t in disc._gdi_tracks(img)]
        for f in files: shutil.copy(f, dst)
        used.add(nxt); print(f"{os.path.basename(img)} -> {os.path.basename(dst)}")

def cmd_sort(a):
    items = [i for i in scan(a.sd) if "error" not in i]
    items.sort(key=lambda i: i["name"].lower())
    tmp = os.path.join(a.sd, "_castmenu_sort"); os.makedirs(tmp)
    for i, it in enumerate(items): shutil.move(it["folder"], os.path.join(tmp, str(i)))
    for i, it in enumerate(items):
        n = i + 2; shutil.move(os.path.join(tmp, str(i)), os.path.join(a.sd, "%02d" % n if n < 100 else "%03d" % n))
    os.rmdir(tmp); print(f"{len(items)} pastas renumeradas")

def main():
    p = argparse.ArgumentParser(prog="castmenu-card", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("scan"); s.add_argument("sd"); s.set_defaults(fn=cmd_scan)
    b = sp.add_parser("build"); b.add_argument("sd"); b.add_argument("--out"); b.add_argument("--menu-bin"); b.add_argument("--themes"); b.add_argument("--covers", help="dir with <PRODUCT>.png custom covers"); b.add_argument("--ip-template"); b.add_argument("--server", help="host:port of the Cast Chat server (or sd/server.txt)"); b.set_defaults(fn=cmd_build)
    ad = sp.add_parser("add"); ad.add_argument("sd"); ad.add_argument("images", nargs="+"); ad.set_defaults(fn=cmd_add)
    so = sp.add_parser("sort"); so.add_argument("sd"); so.set_defaults(fn=cmd_sort)
    a = p.parse_args(); a.fn(a)

if __name__ == "__main__":
    main()
