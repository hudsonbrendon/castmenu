"""Build the menu's 01/ GD-ROM image (disc.gdi + tracks) with mkisofs.

KallistiOS mounts `/cd` from the *low-density* TOC of a GD-ROM, so every file
the menu reads at runtime goes on track01 (LBA 0). Track03 (LBA 45000, high
density) carries IP.BIN + 1ST_READ.BIN for the BIOS, plus a LIST.INI copy for
tools that only look there. Track02 is the mandatory audio stub, placed right
after track01 (GDEMU follows the GDI table literally).
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

def build(out_dir, data_dir, ip_bin_path, volid="CASTMENU", boot_files=("1ST_READ.BIN", "LIST.INI", "CASTMENU.INI")):
    os.makedirs(out_dir, exist_ok=True)
    mk = _mkisofs()
    # low density: everything
    t1 = os.path.join(out_dir, "track01.iso")
    _iso(mk, t1, data_dir, ip_bin_path, volid, 0)
    sectors = (os.path.getsize(t1) + 2047) // 2048
    if sectors > MAX_LOW_SECTORS:
        raise SystemExit(f"menu data too large for the low-density track ({sectors} sectors > {MAX_LOW_SECTORS}); drop covers/themes")
    t2_lba = sectors + 150
    with open(os.path.join(out_dir, "track02.raw"), "wb") as f: f.write(b"\0" * 2352)
    # high density: boot set only
    hd = os.path.join(out_dir, "_hd"); shutil.rmtree(hd, ignore_errors=True); os.makedirs(hd)
    for name in boot_files:
        p = os.path.join(data_dir, name)
        if os.path.exists(p): shutil.copy(p, hd)
    sort = os.path.join(out_dir, "_sort.txt")
    with open(sort, "w") as f: f.write(f"{os.path.join(hd, '1ST_READ.BIN')} 100000\n")
    _iso(mk, os.path.join(out_dir, "track03.iso"), hd, ip_bin_path, volid, 45000, sort)
    shutil.rmtree(hd); os.remove(sort)
    with open(os.path.join(out_dir, "disc.gdi"), "w") as f:
        f.write(f"3\n1 0 4 2048 track01.iso 0\n2 {t2_lba} 0 2352 track02.raw 0\n3 45000 4 2048 track03.iso 0\n")
    return os.path.join(out_dir, "disc.gdi")
