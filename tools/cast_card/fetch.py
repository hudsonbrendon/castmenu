"""Fetch what the disc does not carry: covers from the libretro thumbnail server (CC boxarts),
synopses from Wikipedia (lead section, pt + en), gameplay clips with yt-dlp.
Everything lands next to the game image (cover.png, synopsis.<lang>.txt, video.mp4) so build picks it up."""
import os, re, difflib, urllib.request, urllib.parse

BASE = "https://thumbnails.libretro.com/Sega%20-%20Dreamcast/"
REGION = {"U": "USA", "E": "Europe", "J": "Japan"}

def _index(kind):
    html = urllib.request.urlopen(BASE + kind + "/", timeout=30).read().decode("utf-8", "replace")
    return [urllib.parse.unquote(h) for h in re.findall(r'href="([^"?]+\.png)"', html)]

def _norm(s):
    s = re.sub(r"\.png$", "", s, flags=re.I).lower().replace("&", "and")
    s = re.sub(r"\s*\(.*?\)|\s*\[.*?\]", "", s)
    s = re.sub(r"\b(disc|disco|cd)\s*\d\b", " ", s)
    s = re.sub(r"([a-z])(\d)", r"\1 \2", s)               # 'EVIL3' -> 'evil 3'
    return re.sub(r"[^a-z0-9]+", " ", s).strip()

def _digits(s): return re.findall(r"\d+", s)

def match(title, region, names):
    """Best libretro file name for a title; prefers the game's own region.
    Numbers must agree (so 'Virtua Tennis 2' never falls back to 'Virtua Tennis')."""
    want = _norm(title)
    by = {}
    for n in names: by.setdefault(_norm(n), []).append(n)
    keys = [k for k in by if _digits(k) == _digits(want)]
    hit = want if want in by else None
    if not hit:
        pre = [k for k in keys if k.startswith(want + " ") or want.startswith(k + " ")]
        if pre: hit = min(pre, key=len)
    if not hit:
        close = difflib.get_close_matches(want, keys, n=1, cutoff=0.86)
        hit = close[0] if close else None
    if not hit: return None
    cands = sorted(by[hit], key=len)             # shortest = plain release, no (Rev A)/(Demo) tags
    for r in region or "":
        for c in cands:
            if "(" + REGION.get(r, "?") in c: return c
    return cands[0]

def fetch_shots(items, force=False, log=print):
    """libretro Named_Snaps -> shot1.png (gameplay), Named_Titles -> shot2.png (title screen)."""
    n = 0
    for kind, fname in (("Named_Snaps", "shot1.png"), ("Named_Titles", "shot2.png")):
        try: names = _index(kind)
        except Exception as e: log(f"  !! {kind}: {e}"); continue
        for it in items:
            if "error" in it: continue
            dst = os.path.join(it["folder"], fname)
            if os.path.exists(dst) and not force: continue
            fn = match(it["name"], it["ip"].region, names) or match(it["ip"].name, it["ip"].region, names)
            if not fn: continue
            try:
                data = urllib.request.urlopen(BASE + kind + "/" + urllib.parse.quote(fn), timeout=60).read()
                if data[:8] != b"\x89PNG\r\n\x1a\n": log(f"  ?? {it['slot']:03d} {it['name']}: {kind} is not a PNG"); continue
                open(dst, "wb").write(data); n += 1
                log(f"  ok {it['slot']:03d} {it['name']}: {fname}")
            except Exception as e: log(f"  !! {it['slot']:03d} {it['name']}: {e}")
    return n

def fetch_covers(items, kind="Named_Boxarts", force=False, log=print):
    names = _index(kind)
    n = 0
    for it in items:
        if "error" in it: continue
        dst = os.path.join(it["folder"], "cover.png")
        if os.path.exists(dst) or (it.get("cover") and not force): continue
        fn = match(it["name"], it["ip"].region, names) or match(it["ip"].name, it["ip"].region, names)
        if not fn: log(f"  ?? {it['slot']:03d} {it['name']}: no libretro cover"); continue
        try:
            data = urllib.request.urlopen(BASE + kind + "/" + urllib.parse.quote(fn), timeout=60).read()
            open(dst, "wb").write(data); n += 1
            log(f"  ok {it['slot']:03d} {it['name']} <- {fn}")
        except Exception as e:
            log(f"  !! {it['slot']:03d} {it['name']}: {e}")
    return n

def trim(path, seconds=20):
    """Keep only the window cvm.encode will use (35 % in, stream copy): a 50 MB download becomes ~1 MB on the card."""
    import subprocess, tempfile, shutil
    from . import cvm
    dur = cvm.duration(path)
    if dur <= 30: return
    start = max(0, min(dur * 0.35, dur - seconds))
    tmp = tempfile.mktemp(suffix=".mp4")
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", str(start), "-i", path, "-t", str(seconds), "-an", "-c", "copy", "-movflags", "+faststart", tmp], capture_output=True)
    if r.returncode == 0 and os.path.getsize(tmp) > 10000:
        os.remove(path); shutil.move(tmp, os.path.join(os.path.dirname(path), "video.mp4"))
    elif os.path.exists(tmp): os.remove(tmp)

def fetch_music(items, force=False, log=print):
    """yt-dlp: audio of the first YouTube hit for '<title> dreamcast ost main theme', trimmed to 60 s as music.mp3."""
    import shutil, subprocess
    from . import audio
    if not shutil.which("yt-dlp") or not shutil.which("ffmpeg"): log("  yt-dlp/ffmpeg not found (brew install yt-dlp ffmpeg)"); return 0
    n = 0
    for it in items:
        if "error" in it: continue
        if audio.find_music(it["folder"]) and not force: continue
        q = re.sub(r"\s*(disco?|cd)\s*\d.*$", "", it["name"], flags=re.I)
        out = os.path.join(it["folder"], "music.%(ext)s")
        cmd = ["yt-dlp", "-q", "--no-warnings", "--no-playlist", "--extractor-args", "youtube:player_client=android",
               f"ytsearch6:{q} dreamcast ost main theme", "--match-filter", "duration<420 & duration>45", "--max-downloads", "1",
               "-f", "bestaudio/best", "-x", "--audio-format", "mp3", "--audio-quality", "6", "--force-overwrites", "-o", out]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            got = audio.find_music(it["folder"])
            if got:
                tmp = got + ".tmp.mp3"
                if subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", got, "-t", "60", "-c", "copy", tmp], capture_output=True).returncode == 0 and os.path.getsize(tmp) > 10000:
                    os.remove(got); os.rename(tmp, os.path.join(it["folder"], "music.mp3"))
                n += 1; log(f"  ok {it['slot']:03d} {it['name']}: music")
            else: log(f"  ?? {it['slot']:03d} {it['name']}: no music {(r.stderr or '').strip()[-120:]}")
        except Exception as e:
            log(f"  !! {it['slot']:03d} {it['name']}: {e}"[:200])
    return n

def fetch_videos(items, force=False, log=print):
    """yt-dlp: first YouTube hit for '<title> dreamcast gameplay', a short section from ~40 s in."""
    import shutil, subprocess
    from . import cvm
    if not shutil.which("yt-dlp") or not shutil.which("ffmpeg"): log("  yt-dlp/ffmpeg not found (brew install yt-dlp ffmpeg)"); return 0
    n = 0
    for it in items:
        if "error" in it: continue
        if cvm.find_video(it["folder"]) and not force: continue
        q = re.sub(r"\s*(disco?|cd)\s*\d.*$", "", it["name"], flags=re.I)
        out = os.path.join(it["folder"], "video.%(ext)s")
        # whole video, 360p, no audio: section cuts hand ffmpeg a URL YouTube refuses (403); cvm.encode picks
        # the 12 s window later. Short videos first, then anything under 40 min.
        def grab(search, maxdur):
            cmd = ["yt-dlp", "-q", "--no-warnings", "--no-playlist", "--extractor-args", "youtube:player_client=android",
                   f"{search}:{q} dreamcast gameplay", "--match-filter", f"duration<{maxdur} & duration>60", "--max-downloads", "1",
                   "-f", "bv*[height<=360][ext=mp4]/bv*[height<=360]/b[height<=360]/b/18", "--force-overwrites", "-o", out]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)   # exit 101 = max-downloads reached (fine)
            got = cvm.find_video(it["folder"])
            if got: trim(got)
            return got, (r.stderr or "").strip()[-160:]
        try:
            got, err = grab("ytsearch6", 900)
            if not got: got, err = grab("ytsearch10", 2400)
            if got: n += 1; log(f"  ok {it['slot']:03d} {it['name']}: clip")
            else: log(f"  ?? {it['slot']:03d} {it['name']}: no clip {err}")
        except Exception as e:
            log(f"  !! {it['slot']:03d} {it['name']}: {e}"[:200])
    return n

if __name__ == "__main__":
    names = _index("Named_Boxarts")
    assert match("Sonic Adventure", "U", names) == "Sonic Adventure (USA).png"
    assert match("Dino Crisis", "U", names) == "Dino Crisis (USA).png"
    assert match("NBA 2K", "U", names) == "NBA 2K (USA).png" and match("NBA 2K1", "U", names) == "NBA 2K1 (USA).png"
    assert match("VIRTUA TENNIS 2", "JUE", names).startswith("Virtua Tennis 2")
    assert match("RESIDENT EVIL3", "JUE", names) == "Resident Evil 3 - Nemesis (USA).png"
    assert match("Resident Evil Code - Veronica Disco 2", "JUE", names) == "Resident Evil - Code - Veronica (USA).png"
    assert match("King of Fighters, The - Dream Match 1999", "U", names).startswith("King of Fighters, The - Dream Match 1999")
    assert match("Resident Evil Code - Veronica Disco 1", "JUE", names) is not None
    print("fetch OK")


# ---------- synopses (Wikipedia lead section) ----------
import json
UA = {"User-Agent": "castmenu/0.3 (https://github.com/hudsonbrendon/castmenu)"}
WIKI_LANGS = ("pt", "en")
def _wiki(lang, params):
    url = f"https://{lang}.wikipedia.org/w/api.php?" + urllib.parse.urlencode(dict(params, format="json"))
    for attempt in range(3):
        try: return json.load(urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30))
        except Exception:
            if attempt == 2: raise

def wiki_synopsis(title, lang):
    """Lead section of the Wikipedia article whose title matches the game (digits must agree); None if unsure."""
    want = _norm(title)
    res = _wiki(lang, {"action": "query", "list": "search", "srsearch": f"{title} Dreamcast", "srlimit": 8})["query"]["search"]
    best = None
    for r in res:
        n = _norm(r["title"])
        if _digits(n) != _digits(want): continue
        score = difflib.SequenceMatcher(None, want, n).ratio()
        if n == want or n.startswith(want + " ") and score > 0.6: score += 0.5
        if score >= 0.8 and (not best or score > best[0]): best = (score, r["title"])
    if not best: return None
    pages = _wiki(lang, {"action": "query", "prop": "extracts", "exintro": 1, "explaintext": 1, "redirects": 1, "titles": best[1]})["query"]["pages"]
    txt = list(pages.values())[0].get("extract", "").strip()
    if len(txt) < 80 or not re.search(r"dreamcast|jogo|game|videogame|videojuego", txt, re.I): return None
    txt = re.sub(r"\s*\([^()]*?\b(ドリームキャスト|バイオハザード|[ぁ-んァ-ン一-龯])[^()]*\)", "", txt)   # drop kana/kanji parentheticals
    return re.sub(r"[ \t]+", " ", txt)[:3800]

def fetch_synopses(items, langs=WIKI_LANGS, force=False, log=print):
    n = 0
    for it in items:
        if "error" in it: continue
        for lang in langs:
            dst = os.path.join(it["folder"], f"synopsis.{lang}.txt")
            if os.path.exists(dst) and not force: continue
            try: txt = wiki_synopsis(it["name"], lang)
            except Exception as e: log(f"  {it['name']}: wikipedia {lang} falhou ({e})"); continue
            if not txt: log(f"  {it['name']}: no {lang} article"); continue
            open(dst, "w", encoding="utf-8").write(txt + "\n")
            if lang == "en" and not os.path.exists(os.path.join(it["folder"], "synopsis.txt")):
                open(os.path.join(it["folder"], "synopsis.txt"), "w", encoding="utf-8").write(txt + "\n")   # fallback for the other menu languages
            log(f"  {it['name']}: synopsis {lang} ({len(txt)} chars)"); n += 1
    return n
