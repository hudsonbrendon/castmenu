"""Minimal ISO9660 reader over a sector-addressable source (GDI/CDI data track)."""
import struct

class SectorSource:
    """read(lba) -> 2048 user bytes. Subclasses map LBA -> file offset."""
    def read(self, lba, count=1): raise NotImplementedError

class FileSectorSource(SectorSource):
    def __init__(self, path, start_lba, sector_size=2048, data_offset=0, file_offset=0):
        self.f = open(path, "rb"); self.start = start_lba; self.ss = sector_size; self.doff = data_offset; self.foff = file_offset
    def read(self, lba, count=1):
        out = bytearray()
        for i in range(count):
            self.f.seek(self.foff + (lba - self.start + i) * self.ss + self.doff)
            out += self.f.read(2048)
        return bytes(out)
    def close(self): self.f.close()

class MultiTrackSource(SectorSource):
    """Several data tracks of one disc (GDI): maps each LBA to the track that holds it.
    Game files often live on a later data track than the directory that names them."""
    def __init__(self, tracks):
        self.tracks = sorted(tracks, key=lambda t: t["lba"]); self.start = self.tracks[0]["lba"]
        self.files = {}
    def read(self, lba, count=1):
        out = bytearray()
        for i in range(count):
            l = lba + i; t = None
            for cand in self.tracks:
                if cand["lba"] <= l: t = cand
            if t is None: out += b"\0" * 2048; continue
            f = self.files.get(t["file"])
            if f is None: f = self.files[t["file"]] = open(t["file"], "rb")
            f.seek(t["off"] + (l - t["lba"]) * t["ss"] + (0 if t["ss"] == 2048 else 16))
            out += f.read(2048).ljust(2048, b"\0")
        return bytes(out)
    def close(self):
        for f in self.files.values(): f.close()
        self.files = {}

def _dirent(rec):
    ln = rec[0]
    if ln < 33: return None
    extent = struct.unpack("<I", rec[2:6])[0]; size = struct.unpack("<I", rec[10:14])[0]
    flags = rec[25]; nl = rec[32]; name = rec[33:33 + nl]
    if name == b"\x00": name = b"."
    elif name == b"\x01": name = b".."
    n = name.decode("ascii", "replace").split(";")[0]
    return dict(name=n, extent=extent, size=size, is_dir=bool(flags & 2), length=ln)

def list_dir(src, extent, size):
    out = []
    nsec = (size + 2047) // 2048
    data = src.read(extent, nsec)
    pos = 0
    while pos < len(data):
        if data[pos] == 0:
            pos = (pos // 2048 + 1) * 2048; continue   # padding to next sector
        e = _dirent(data[pos:pos + data[pos]])
        if e is None: break
        out.append(e); pos += e["length"]
    return out

def root_dir(src):
    pvd = src.read(src.start + 16)
    if pvd[1:6] != b"CD001": raise ValueError("no PVD")
    rec = pvd[156:156 + 34]
    e = _dirent(rec)
    return e["extent"], e["size"], pvd[40:72].decode("ascii", "replace").strip()

def find_file(src, name, extent=None, size=None):
    if extent is None: extent, size, _ = root_dir(src)
    for e in list_dir(src, extent, size):
        if e["name"].upper() == name.upper() and not e["is_dir"]: return e
    return None

def read_file(src, entry):
    return src.read(entry["extent"], (entry["size"] + 2047) // 2048)[:entry["size"]]
