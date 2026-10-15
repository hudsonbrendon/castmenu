"""Disc image readers: .gdi, .cdi, .iso -> IP.BIN bytes + optional 0GDTEX.PVR."""
import os, re, struct
from . import iso9660, ipbin

SIG = ipbin.SIG

class DiscInfo:
    def __init__(self, path, kind, ip, cover=None):
        self.path, self.kind, self.ip, self.cover = path, kind, ip, cover

def _gdi_tracks(gdi_path):
    base = os.path.dirname(gdi_path)
    tracks = []
    with open(gdi_path, "r", errors="replace") as f:
        lines = [l.strip() for l in f if l.strip()]
    for l in lines[1:]:
        m = re.match(r'(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+("[^"]+"|\S+)\s+(\d+)', l)
        if not m: continue
        num, lba, ctrl, ss, fn, off = m.groups()
        tracks.append(dict(num=int(num), lba=int(lba), ctrl=int(ctrl), ss=int(ss), file=os.path.join(base, fn.strip('"')), off=int(off)))
    return tracks

def read_gdi(gdi_path):
    tracks = _gdi_tracks(gdi_path)
    data = [t for t in tracks if t["ctrl"] & 4]
    if not data: raise ValueError("no data track")
    t = data[-1]                                 # high-density data track (LBA 45000 for GD-ROM)
    doff = 0 if t["ss"] == 2048 else 16
    src = iso9660.FileSectorSource(t["file"], t["lba"], t["ss"], doff, t["off"])
    ip = src.read(t["lba"], 16)
    if ip[:16] != SIG:                            # some dumps keep IP.BIN on the first data track
        t = data[0]; src.close()
        src = iso9660.FileSectorSource(t["file"], t["lba"], t["ss"], 0 if t["ss"] == 2048 else 16, t["off"])
        ip = src.read(t["lba"], 16)
    cover = None
    src.close()
    for t2 in reversed(data):                     # cover usually sits on the boot track; check the others too
        s2 = iso9660.FileSectorSource(t2["file"], t2["lba"], t2["ss"], 0 if t2["ss"] == 2048 else 16, t2["off"])
        try:
            e = iso9660.find_file(s2, "0GDTEX.PVR")
            if e and e["size"] < 2_000_000: cover = iso9660.read_file(s2, e)
        except Exception: pass
        s2.close()
        if cover: break
    return DiscInfo(gdi_path, "gdi", ipbin.parse(ip), cover)

def _find_sig(path, chunk=32 << 20):
    """Locate the first IP.BIN signature at a plausible sector boundary. Scans the whole
    file in chunks (DiscJuggler images put the data track after the audio session, hundreds of MB in)."""
    with open(path, "rb") as f:
        base = 0; tail = b""
        while True:
            data = f.read(chunk)
            if not data: break
            buf = tail + data
            pos = buf.find(SIG)
            while pos >= 0:
                abs_pos = base - len(tail) + pos
                for ss, doff in ((2048, 0), (2352, 16), (2336, 8), (2352, 24)):
                    if (abs_pos - doff) % ss == 0: return abs_pos, ss, doff
                # sessions before the data track may use other sector sizes, so the track is not
                # aligned to the file start; the header sector alone is enough for IP.BIN fields
                return abs_pos, 2352, 16
            tail = buf[-len(SIG):]; base += len(data)
    return -1, 0, 0

def read_cdi_or_iso(path):
    """Heuristic reader for CDI (DiscJuggler) and plain ISO: no descriptor parsing,
    the data track is located by its IP.BIN signature and sector geometry."""
    pos, ss, doff = _find_sig(path)
    if pos < 0: raise ValueError("IP.BIN not found")
    with open(path, "rb") as f:
        f.seek(pos); ip = f.read(0x8000)
    kind = "cdi" if path.lower().endswith(".cdi") else "iso"
    cover = None
    try:
        # Try common track start LBAs; validate via the root directory's self-entry.
        for lba0 in (0, 11702, 11700, 45000, 150):
            src = iso9660.FileSectorSource(path, lba0, ss, doff, pos - doff)
            try:
                ext, size, _ = iso9660.root_dir(src)
                first = src.read(ext)
                if first[0] >= 33 and first[32] == 1 and first[33] == 0 and struct.unpack("<I", first[2:6])[0] == ext:
                    e = iso9660.find_file(src, "0GDTEX.PVR", ext, size)
                    if e and e["size"] < 2_000_000: cover = iso9660.read_file(src, e)
                    src.close(); break
            except Exception: pass
            src.close()
    except Exception: pass
    return DiscInfo(path, kind, ipbin.parse(ip), cover)

IMAGE_EXTS = (".gdi", ".cdi", ".iso")

def find_image(folder):
    files = sorted(os.listdir(folder))
    for ext in IMAGE_EXTS:
        for fn in files:
            if fn.lower().endswith(ext): return os.path.join(folder, fn)
    return None

def read_image(path):
    return read_gdi(path) if path.lower().endswith(".gdi") else read_cdi_or_iso(path)
