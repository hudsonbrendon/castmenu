"""Build the menu's 01/ GD-ROM image (disc.gdi + tracks) with mkisofs.

KallistiOS mounts `/cd` from the *low-density* TOC of a GD-ROM, so every file
the menu reads at runtime goes on track01 (LBA 0). Track03 (LBA 45000, high
density) carries IP.BIN + 1ST_READ.BIN for the BIOS, plus a LIST.INI copy for
tools that only look there. Track02 is the mandatory audio stub, placed right
after track01 (GDEMU follows the GDI table literally). The high-density area mirrors GDMENU/openMenu:
track03 at 45000 holds IP.BIN and the ISO9660 structures, track04 is an audio stub and track05 at
544180 holds the file data; the real BIOS only boots from the last data track (3-track GDIs boot in
redream's HLE BIOS but not on the console).
"""
import os, shutil, subprocess

MAX_LOW_SECTORS = 44_000   # keep clear of the high-density area at 45 000

def _mkisofs():
    for c in ("mkisofs", "genisoimage", "xorrisofs"):
        if shutil.which(c): return c
    raise SystemExit("mkisofs/genisoimage not found (brew install cdrtools)")

def _iso(mk, out, src, ip_bin_path, volid, start_lba, sort=None):
    cmd = [mk, "-quiet", "-C", f"0,{start_lba}", "-V", volid, "-G", ip_bin_path, "-r", "-J", "-l"]
    if sort: cmd += ["-sort", sort]
    subprocess.check_call(cmd + ["-o", out, src])

HD_LBA = 45_000          # high-density area (track03: IP.BIN + ISO9660 structures)
AUDIO2_LBA = 60_000      # track04 audio stub
DATA_LBA = 544_180       # track05: file data (the BIOS boots from the LAST data track; GDMENU/openMenu layout)
GD_END_LBA = 548_000     # a GD-ROM's high-density area ends at ~549 150; GDEMU rejects images that run past it
                         # (no disc at all), so big track05s slide back to keep the end inside the disc

def _write_contig(path, data):
    """Write a track file preallocated contiguously (macOS msdosfs honours F_ALLOCATECONTIG); GDEMU
    firmware reads cluster chains and a fragmented 100 MB track on a full card shows up as 'no disc'."""
    import fcntl, struct
    if os.path.exists(path): os.remove(path)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    try:
        for flags in (2 | 4, 4):   # F_ALLOCATECONTIG|F_ALLOCATEALL, then F_ALLOCATEALL
            try: fcntl.fcntl(fd, 42, struct.pack("IIqqq", flags, 3, 0, len(data), 0)); break   # F_PREALLOCATE, F_PEOFPOSMODE
            except OSError: continue
    except Exception: pass
    off = 0
    while off < len(data): off += os.write(fd, data[off:])
    os.fsync(fd); os.close(fd)

def _both(le, be_off, buf, value):
    buf[le:le + 4] = value.to_bytes(4, "little"); buf[be_off:be_off + 4] = value.to_bytes(4, "big")

def _split_hd(iso_path, t3_path, t5_path, data_lba=DATA_LBA):
    """Split a flat ISO built at LBA 45000 into GDMENU's shape: structures in track03, file
    extents relocated to track05 at DATA_LBA. Root directory only (the boot set is flat)."""
    img = bytearray(open(iso_path, "rb").read())
    pvd = img[16 * 2048:17 * 2048]
    root_ext = int.from_bytes(pvd[156 + 2:156 + 6], "little"); root_size = int.from_bytes(pvd[156 + 10:156 + 14], "little")
    base = (root_ext - HD_LBA) * 2048
    recs = []; off = base
    while off < base + root_size:
        if img[off] == 0: off = (off // 2048 + 1) * 2048; continue
        l = img[off]; flags = img[off + 25]; ext = int.from_bytes(img[off + 2:off + 6], "little"); nl = img[off + 32]
        if not (flags & 2) and nl and img[off + 33] >= 32: recs.append((off, ext))
        off += l
    if not recs: raise SystemExit("no files in the high-density boot set")
    first = min(e for _, e in recs)
    header_sectors = first - HD_LBA
    for off, ext in recs: _both(off + 2, off + 6, img, ext - first + data_lba)
    data = img[header_sectors * 2048:]
    data_sectors = (len(data) + 2047) // 2048
    _both(80, 84, img, data_lba + data_sectors - HD_LBA)   # volume space size (relative to the HD area)
    _write_contig(t3_path, bytes(img[:header_sectors * 2048]))
    return header_sectors, data_sectors, bytes(data.ljust(data_sectors * 2048, b"\0"))

def build(out_dir, data_dir, ip_bin_path, volid="CASTMENU", boot_files=("1ST_READ.BIN", "LIST.INI", "CASTMENU.INI"), videos=(), music=()):
    """videos: [(slot, path to .cvm)] appended raw after the boot set on track05 (the low-density area is
    only ~88 MB); the menu reads them by sector through /cd/videos.idx: 'slot lba frames fps w h'.
    music: [(slot, path to .cma)] likewise, indexed by /cd/music.idx: 'slot lba bytes rate' (slots >= 1000 = menu tracks)."""
    os.makedirs(out_dir, exist_ok=True)
    mk = _mkisofs()
    # high density first: the videos.idx written below must land on track01
    hd = os.path.join(out_dir, "_hd"); shutil.rmtree(hd, ignore_errors=True); os.makedirs(hd)
    for name in boot_files:
        p = os.path.join(data_dir, name)
        if os.path.exists(p): shutil.copy(p, hd)
    flat = os.path.join(out_dir, "_hd.iso")
    subprocess.check_call([mk, "-quiet", "-C", f"0,{HD_LBA}", "-V", volid, "-G", ip_bin_path, "-o", flat, hd])
    clips = []
    for slot, path in videos:
        blob = open(path, "rb").read()
        if blob[:4] == b"CVID": clips.append((slot, blob + b"\0" * ((-len(blob)) % 2048)))
    tracks = []
    for slot, path in music:
        blob = open(path, "rb").read()
        if blob[:4] == b"CAUD": tracks.append((slot, blob + b"\0" * ((-len(blob)) % 2048)))
    total = (os.path.getsize(flat) + 2047) // 2048 + sum(len(b) for _, b in clips) // 2048 + sum(len(b) for _, b in tracks) // 2048
    data_lba = DATA_LBA if DATA_LBA + total <= GD_END_LBA else max(AUDIO2_LBA + 600, GD_END_LBA - total)
    t5 = os.path.join(out_dir, "track05.iso")
    _, data_sectors, t5_data = _split_hd(flat, os.path.join(out_dir, "track03.iso"), t5, data_lba)
    shutil.rmtree(hd); os.remove(flat)
    lines, parts = [], [t5_data]
    for slot, blob in clips:
        w, h, n, fps = (int.from_bytes(blob[4 + 2 * i:6 + 2 * i], "little") for i in range(4))
        lines.append(f"{slot} {data_lba + data_sectors} {n} {fps} {w} {h}")
        parts.append(blob); data_sectors += len(blob) // 2048
    mlines = []
    for slot, blob in tracks:
        rate, nbytes = int.from_bytes(blob[4:8], "little"), int.from_bytes(blob[8:12], "little")
        mlines.append(f"{slot} {data_lba + data_sectors} {nbytes} {rate}")
        parts.append(blob); data_sectors += len(blob) // 2048
    _write_contig(t5, b"".join(parts))
    if lines: open(os.path.join(data_dir, "videos.idx"), "w").write("\n".join(lines) + "\n")
    if mlines: open(os.path.join(data_dir, "music.idx"), "w").write("\n".join(mlines) + "\n")
    # low density: everything the menu reads at runtime
    t1 = os.path.join(out_dir, "track01.iso")
    _iso(mk, t1, data_dir, ip_bin_path, volid, 0)
    _write_contig(t1, open(t1, "rb").read())
    sectors = (os.path.getsize(t1) + 2047) // 2048
    if sectors > MAX_LOW_SECTORS:
        raise SystemExit(f"menu data too large for the low-density track ({sectors} sectors > {MAX_LOW_SECTORS}); drop covers/themes")
    t2_lba = sectors + 150
    with open(os.path.join(out_dir, "track02.raw"), "wb") as f: f.write(b"\0" * 2352)
    with open(os.path.join(out_dir, "track04.raw"), "wb") as f: f.write(b"\0" * 2352)
    with open(os.path.join(out_dir, "disc.gdi"), "w") as f:
        f.write(f"5\n1 0 4 2048 track01.iso 0\n2 {t2_lba} 0 2352 track02.raw 0\n3 {HD_LBA} 4 2048 track03.iso 0\n4 {AUDIO2_LBA} 0 2352 track04.raw 0\n5 {data_lba} 4 2048 track05.iso 0\n")
    return os.path.join(out_dir, "disc.gdi")

def read_hd_file(out_dir, name):
    """Read a boot-set file back from a built 01/ (directory in track03, data in track05)."""
    data_lba = int(open(os.path.join(out_dir, "disc.gdi")).read().splitlines()[-1].split()[1])
    t3 = open(os.path.join(out_dir, "track03.iso"), "rb").read()
    pvd = t3[16 * 2048:17 * 2048]; root = int.from_bytes(pvd[156 + 2:156 + 6], "little"); size = int.from_bytes(pvd[156 + 10:156 + 14], "little")
    rd = t3[(root - HD_LBA) * 2048:(root - HD_LBA) * 2048 + size]; off = 0
    while off < len(rd) and rd[off]:
        l = rd[off]; ext = int.from_bytes(rd[off + 2:off + 6], "little"); sz = int.from_bytes(rd[off + 10:off + 14], "little"); nl = rd[off + 32]
        if rd[off + 33:off + 33 + nl].decode("ascii", "replace").split(";")[0].upper() == name.upper():
            with open(os.path.join(out_dir, "track05.iso"), "rb") as f: f.seek((ext - data_lba) * 2048); return f.read(sz)
        off += l
    return None
