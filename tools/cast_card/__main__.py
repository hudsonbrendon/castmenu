"""castmenu-card: prepare a GDEMU SD card for the Cast Menu menu.

  python3 -m tools.cast_card scan  <sd_root>                 list what is on the card
  python3 -m tools.cast_card build <sd_root> [--out DIR]     write LIST.INI/CASTMENU.INI, covers, themes, 01/ menu image
  python3 -m tools.cast_card add   <sd_root> <image>...      copy images into the next free folders (then build)
  python3 -m tools.cast_card sort  <sd_root>                 renumber folders alphabetically (02..)
  python3 -m tools.cast_card fetch <sd_root> [--force]       download missing covers (libretro thumbnails) as cover.png
  python3 -m tools.cast_card fetch <sd_root> --videos        also grab a gameplay clip per game with yt-dlp (video.mp4)
  python3 -m tools.cast_card fetch <sd_root> --music         also grab a theme / OST clip per game (music.mp3)

Folders follow the GDEMU rule: 01 = menu, 02..999 = one image per folder.

Per-game extras next to the image: name.txt, cover.png, shot1.png/shot2.png (screenshots), video.mp4 (background clip),
music.mp3 (theme), collection.txt, cheats.txt, genre.txt, year.txt, players.txt,
publisher.txt, category.txt (or platform.txt), synopsis.txt / synopsis.<pt|en|es|de|fr|it>.txt.
Card root extras: avatars/*.png (profile pictures), music/*.mp3 (menu background tracks), cheats/<PRODUCT>.txt,
server.txt (host:port of the Cast Chat server).
"""
import argparse, os, re, shutil, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.cast_card import disc, ipbin, gdi, tex, themes, platforms, romfs, cvm, audio
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
        for col in it.get("collections", []): nb.append(f"{s}.collection={col}")
        nb.append("")
    return "\n".join(gd) + "\n", "\n".join(nb) + "\n"

def read_collections(sd, items):
    """collections/<Name>.txt on the card root (lines: folder number, product id or exact title) and
    collection.txt in a game folder (one collection name per line) -> it["collections"]."""
    for it in items: it["collections"] = []
    d = os.path.join(sd, "collections")
    if os.path.isdir(d):
        for fn in sorted(os.listdir(d)):
            if not fn.lower().endswith(".txt"): continue
            name = os.path.splitext(fn)[0][:19]
            keys = {l.strip().lower() for l in open(os.path.join(d, fn), encoding="utf-8", errors="replace") if l.strip() and not l.startswith("#")}
            for it in items:
                if "error" in it: continue
                ids = {"%02d" % it["slot"], "%03d" % it["slot"], str(it["slot"]), it["name"].lower(), it["ip"].product.replace(" ", "").replace("-", "").lower(), it["ip"].product.lower()}
                if ids & keys: it["collections"].append(name)
    for it in items:
        if "error" in it: continue
        p = os.path.join(it["folder"], "collection.txt")
        if os.path.exists(p):
            for l in open(p, encoding="utf-8", errors="replace"):
                if l.strip() and l.strip()[:19] not in it["collections"]: it["collections"].append(l.strip()[:19])

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
            open(os.path.join(d, "%03d.txt" % it["slot"]), "w", encoding="latin-1", errors="replace").write(txt[:3800])

def write_avatars(sd, data_dir):
    src = os.path.join(sd, "avatars")
    if not os.path.isdir(src): return
    d = os.path.join(data_dir, "avatars"); os.makedirs(d, exist_ok=True)
    for f in sorted(os.listdir(src)):
        if not f.lower().endswith((".png", ".jpg", ".jpeg")): continue
        name = re.sub(r"[^a-z0-9_-]", "", os.path.splitext(f)[0].lower())[:15]
        img = Image.open(os.path.join(src, f)).convert("RGB").resize((64, 64), Image.LANCZOS)
        open(os.path.join(d, name + ".tex"), "wb").write(tex.encode(img, "rgb565"))

def find_gdmenu_loader(sd):
    """GDMENU's own loader.bin: sd/loader.bin, or lifted from a GDMENU image in sd/gdmenu/ or sd/01-gdmenu-full/."""
    direct = os.path.join(sd, "loader.bin")
    if os.path.exists(direct): return open(direct, "rb").read()
    for d in ("gdmenu", "01-gdmenu-full", "GDMENU"):
        folder = os.path.join(sd, d)
        if not os.path.isdir(folder): continue
        for f in sorted(os.listdir(folder)):
            if f.lower().endswith((".iso", ".bin")):
                blob = romfs.extract(open(os.path.join(folder, f), "rb").read(), "loader.bin")
                if blob: return blob
    return None

def placeholder_cover(title, size=128):
    """Shell-grey card, orange swirl, title in the Dreamcast face: for homebrew/hacks with no art anywhere."""
    from PIL import ImageDraw, ImageFont
    img = Image.new("RGB", (size, size), (0x26, 0x27, 0x2A))
    d = ImageDraw.Draw(img)
    for y in range(size): d.line([(0, y), (size, y)], fill=(0x33 + y // 6, 0x34 + y // 6, 0x38 + y // 6))
    try:
        sw = Image.open(os.path.join(ROOT, "assets", "brand", "swirl.png")).convert("RGBA").resize((40, 40), Image.LANCZOS)
        img.paste(sw, ((size - 40) // 2, 14), sw)
    except Exception: pass
    try: font = ImageFont.truetype(os.path.join(ROOT, "assets", "fonts", "Dreamcast.ttf", layout_engine=ImageFont.Layout.BASIC), 13)
    except Exception: font = ImageFont.load_default()
    words, lines, cur = title.upper().split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if d.textlength(t, font=font) <= size - 16: cur = t
        else: lines.append(cur); cur = w
    lines.append(cur); lines = [l for l in lines if l][:4]
    y = 64
    for l in lines: d.text(((size - d.textlength(l, font=font)) / 2, y), l, font=font, fill=(0xF2, 0x65, 0x22)); y += 15
    d.rectangle([0, 0, size - 1, size - 1], outline=(0x66, 0x69, 0x6D))
    return img

def video_clip(folder, redo=False):
    """video.cvm next to the image, (re)encoded from video.mp4/mkv/... when missing or older (or redo)."""
    src = cvm.find_video(folder)
    if not src: return None
    dst = os.path.join(folder, "video.cvm")
    if redo or not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
        print(f"  video {os.path.basename(folder)}: {os.path.basename(src)}")
        try: cvm.encode(src, dst)
        except Exception as e: print(f"  !! {os.path.basename(folder)}: video: {e}"); return None
    return dst

def music_clip(folder, redo=False):
    """music.cma next to the image, (re)encoded from music.mp3/ogg/... when missing or older (or redo)."""
    src = audio.find_music(folder)
    if not src: return None
    dst = os.path.join(folder, "music.cma")
    if redo or not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
        print(f"  music {os.path.basename(folder)}: {os.path.basename(src)}")
        try: audio.encode(src, dst)
        except Exception as e: print(f"  !! {os.path.basename(folder)}: music: {e}"); return None
    return dst

def menu_tracks(sd, redo=False):
    """sd/music/*.mp3|ogg|wav -> sd/music/.cma/<name>.cma, returned as [(1000 + i, path)]."""
    d = os.path.join(sd, "music")
    if not os.path.isdir(d): return []
    out = []
    for i, fn in enumerate(sorted(f for f in os.listdir(d) if f.lower().endswith(audio.MUSIC_EXTS))):
        src = os.path.join(d, fn); cache = os.path.join(d, ".cma"); os.makedirs(cache, exist_ok=True)
        dst = os.path.join(cache, os.path.splitext(fn)[0] + ".cma")
        if redo or not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
            print(f"  menu track: {fn}")
            try: audio.encode(src, dst, seconds=120)
            except Exception as e: print(f"  !! {fn}: {e}"); continue
        out.append((1000 + i, dst))
    return out

def build_data(items, data_dir, menu_bin, themes_dir, custom_covers=None, sd=None, server=None, gdmenu_loader=None, redo=False, music_redo=False):
    os.makedirs(os.path.join(data_dir, "covers"), exist_ok=True)
    shutil.copy(menu_bin, os.path.join(data_dir, "1ST_READ.BIN"))
    if gdmenu_loader:
        open(os.path.join(data_dir, "loader.bin"), "wb").write(gdmenu_loader); print("  GDMENU loader.bin included (Settings > Loader > GDMENU)")
    if sd and not server:
        st = os.path.join(sd, "server.txt")
        if os.path.exists(st): server = open(st).read().strip()
    if sd: read_collections(sd, items)
    for it in items:                                     # cheats.txt in the game folder, else cheats/<PRODUCT>.txt on the card root
        if "error" in it: continue
        srcs = [os.path.join(it["folder"], "cheats.txt")]
        if sd: srcs.append(os.path.join(sd, "cheats", it["ip"].product.replace(" ", "").replace("-", "") + ".txt"))
        for p in srcs:
            if os.path.exists(p):
                os.makedirs(os.path.join(data_dir, "cheats"), exist_ok=True)
                txt = open(p, encoding="utf-8", errors="replace").read()
                open(os.path.join(data_dir, "cheats", "%03d.txt" % it["slot"]), "w", encoding="latin-1", errors="replace").write(txt); break
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
        if not png and not it.get("cover"):           # same disc under another folder/hack: borrow its art
            for o in items:
                if o is not it and "error" not in o and o["ip"].product == it["ip"].product and o["ip"].product:
                    for ext in ("png", "jpg", "jpeg"):
                        if os.path.exists(os.path.join(o["folder"], "cover." + ext)): png = os.path.join(o["folder"], "cover." + ext)
                    if not png and o.get("cover"): it["cover"] = o["cover"]
                    if png or it.get("cover"): break
        if png:
            img = Image.open(png).convert("RGB").resize((128, 128), Image.LANCZOS)
            open(dst + ".tex", "wb").write(tex.encode(img, "rgb565")); it["cover"] = True
        elif it.get("cover"):
            open(dst + ".pvr", "wb").write(it["cover"])
        else:                                         # nothing anywhere: a generated card with the title
            open(dst + ".tex", "wb").write(tex.encode(placeholder_cover(it["name"]), "rgb565")); it["cover"] = True
        for k in (1, 2):
            p = os.path.join(it["folder"], f"shot{k}.png")
            if os.path.exists(p):
                os.makedirs(os.path.join(data_dir, "shots"), exist_ok=True)
                img = Image.open(p).convert("RGB"); img = img.resize((256, 192), Image.LANCZOS)
                open(os.path.join(data_dir, "shots", "%03d_%d.tex" % (it["slot"], k)), "wb").write(tex.encode(img, "rgb565"))
        it["cvm"] = video_clip(it["folder"], redo)
        it["cma"] = music_clip(it["folder"], music_redo)
    # themes: convert bg.png -> bg.tex (512x512) and copy ini
    for tid in sorted(os.listdir(themes_dir)):
        src = os.path.join(themes_dir, tid)
        if not os.path.isdir(src) or not os.path.exists(os.path.join(src, "theme.ini")): continue
        dst = os.path.join(data_dir, "themes", tid); os.makedirs(dst, exist_ok=True)
        shutil.copy(os.path.join(src, "theme.ini"), dst)
        for snd in ("move.wav", "select.wav", "back.wav"):
            if os.path.exists(os.path.join(src, snd)): shutil.copy(os.path.join(src, snd), dst)
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
    print(f"{len(good)} games found")
    write_meta(good)
    menu_bin = a.menu_bin or os.path.join(ROOT, "1ST_READ.BIN")
    if not os.path.exists(menu_bin): sys.exit(f"menu binary not found: {menu_bin} (run ./build.sh first)")
    out = a.out or os.path.join(a.sd, "01")
    with tempfile.TemporaryDirectory() as tmp:
        data = os.path.join(tmp, "data"); os.makedirs(data)
        gl = open(a.gdmenu_loader, "rb").read() if getattr(a, "gdmenu_loader", None) else find_gdmenu_loader(a.sd)
        build_data(items, data, menu_bin, a.themes or os.path.join(ROOT, "themes"), a.covers, sd=a.sd, server=getattr(a, "server", None), gdmenu_loader=gl, redo=getattr(a, "videos_redo", False), music_redo=getattr(a, "music_redo", False))
        tracks = menu_tracks(a.sd, getattr(a, "music_redo", False))
        ip_tmpl = open(a.ip_template or os.path.join(ROOT, "assets", "IP.BIN"), "rb").read()
        ip = ipbin.patch(ip_tmpl, name="CASTMENU", product="CASTMENU", version="V1.000", date="20261015", company="CASTMENU")
        ipp = os.path.join(tmp, "IP.BIN"); open(ipp, "wb").write(ip)
        if os.path.isdir(out):
            for f in os.listdir(out):
                try: os.remove(os.path.join(out, f))
                except FileNotFoundError: pass   # macOS drops ._AppleDouble files together with their owner
        gdi.build(out, data, ipp, videos=[(it["slot"], it["cvm"]) for it in good if it.get("cvm")], music=[(it["slot"], it["cma"]) for it in good if it.get("cma")] + tracks)
    cfg = os.path.join(a.sd, "GDEMU.ini")
    if not os.path.exists(cfg):
        open(cfg, "w").write("open_time = 150\ndetect_time = 150\nreset_goto = 1\n")
    print(f"menu written to {out}")

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

def cmd_fetch(a):
    from tools.cast_card import fetch
    items = scan(a.sd)
    n = fetch.fetch_covers(items, force=a.force)
    print(f"{n} covers downloaded; run build to write them")
    n = fetch.fetch_shots(items, force=a.force)
    print(f"{n} screenshots downloaded; run build to write them")
    if not a.no_synopses:
        n = fetch.fetch_synopses(items, force=a.force)
        print(f"{n} synopses downloaded (Wikipedia); run build to write them")
    if a.videos:
        n = fetch.fetch_videos(items, force=a.force)
        print(f"{n} clips downloaded; run build to write them")
    if a.music:
        n = fetch.fetch_music(items, force=a.force)
        print(f"{n} music clips downloaded; run build to write them")

def cmd_sort(a):
    items = [i for i in scan(a.sd) if "error" not in i]
    items.sort(key=lambda i: i["name"].lower())
    tmp = os.path.join(a.sd, "_castmenu_sort"); os.makedirs(tmp)
    for i, it in enumerate(items): shutil.move(it["folder"], os.path.join(tmp, str(i)))
    for i, it in enumerate(items):
        n = i + 2; shutil.move(os.path.join(tmp, str(i)), os.path.join(a.sd, "%02d" % n if n < 100 else "%03d" % n))
    os.rmdir(tmp); print(f"{len(items)} folders renumbered")

def main():
    p = argparse.ArgumentParser(prog="castmenu-card", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("scan"); s.add_argument("sd"); s.set_defaults(fn=cmd_scan)
    b = sp.add_parser("build"); b.add_argument("sd"); b.add_argument("--out"); b.add_argument("--menu-bin"); b.add_argument("--themes"); b.add_argument("--covers", help="dir with <PRODUCT>.png custom covers"); b.add_argument("--ip-template"); b.add_argument("--videos-redo", action="store_true", help="re-encode every clip (after encoder changes)"); b.add_argument("--music-redo", action="store_true", help="re-encode every music clip"); b.add_argument("--server", help="host:port of the Cast Chat server (or sd/server.txt)"); b.add_argument("--gdmenu-loader", help="GDMENU loader.bin (or a GDMENU 1ST_READ.BIN/track file) to ship as the alternative loader"); b.set_defaults(fn=cmd_build)
    ad = sp.add_parser("add"); ad.add_argument("sd"); ad.add_argument("images", nargs="+"); ad.set_defaults(fn=cmd_add)
    so = sp.add_parser("sort"); so.add_argument("sd"); so.set_defaults(fn=cmd_sort)
    fe = sp.add_parser("fetch"); fe.add_argument("sd"); fe.add_argument("--force", action="store_true", help="also for games that carry 0GDTEX.PVR / already have a clip"); fe.add_argument("--videos", action="store_true", help="download a gameplay clip per game (needs yt-dlp + ffmpeg)"); fe.add_argument("--music", action="store_true", help="download a theme / OST clip per game as music.mp3 (needs yt-dlp + ffmpeg)"); fe.add_argument("--no-synopses", action="store_true", help="skip the Wikipedia synopses"); fe.set_defaults(fn=cmd_fetch)
    a = p.parse_args(); a.fn(a)

if __name__ == "__main__":
    main()
