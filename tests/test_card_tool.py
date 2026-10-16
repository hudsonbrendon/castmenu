"""Self-check for the card tool: builds a fake GDI game with mkisofs, then
scans it, generates LIST.INI/CASTMENU.INI and builds a menu image.
Run: python3 tests/test_card_tool.py"""
import os, sys, shutil, subprocess, tempfile
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, ROOT)
from tools.cast_card import disc, ipbin, gdi, __main__ as cli

def fake_game(folder, name, product, region="JU", cover=True):
    os.makedirs(folder)
    data = os.path.join(folder, "_data"); os.makedirs(data)
    open(os.path.join(data, "1ST_READ.BIN"), "wb").write(b"\x09\x00" * 64)
    if cover:
        # minimal PVR header (twiddled RGB565 8x8) + 128 bytes
        open(os.path.join(data, "0GDTEX.PVR"), "wb").write(b"PVRT" + (8 + 128).to_bytes(4, "little") + b"\x01\x01\x00\x00" + (8).to_bytes(2, "little") + (8).to_bytes(2, "little") + b"\xAA" * 128)
    ip = ipbin.patch(open(os.path.join(ROOT, "assets", "IP.BIN"), "rb").read(), name=name, product=product, region=region, date="19990909", vga=True)
    ipp = os.path.join(folder, "_ip.bin"); open(ipp, "wb").write(ip)
    # retail-style 3-track GDI: files live in track03 itself
    gdi._iso(gdi._mkisofs(), os.path.join(folder, "track01.iso"), data, ipp, name[:8], 0)
    open(os.path.join(folder, "track02.raw"), "wb").write(b"\0" * 2352)
    gdi._iso(gdi._mkisofs(), os.path.join(folder, "track03.iso"), data, ipp, name[:8], 45000)
    open(os.path.join(folder, "disc.gdi"), "w").write("3\n1 0 4 2048 track01.iso 0\n2 600 0 2352 track02.raw 0\n3 45000 4 2048 track03.iso 0\n")
    shutil.rmtree(data); os.remove(ipp)

with tempfile.TemporaryDirectory() as sd:
    fake_game(os.path.join(sd, "02"), "SONIC ADVENTURE", "MK-51000", "JUE")
    fake_game(os.path.join(sd, "03"), "SHENMUE", "HDR-0016", "E", cover=False)
    fake_game(os.path.join(sd, "04"), "SHENMUE DISC 2", "HDR-0016", "E")
    have_video = bool(shutil.which("ffmpeg"))
    if have_video: subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=30", "-t", "1.5", "-pix_fmt", "yuv420p", os.path.join(sd, "02", "video.mp4")], check=True)
    items = cli.scan(sd)
    assert [i["slot"] for i in items] == [2, 3, 4], items
    assert items[0]["ip"].name == "SONIC ADVENTURE" and items[0]["ip"].region == "JUE" and items[0]["ip"].vga
    assert items[0]["cover"] and items[0]["cover"][:4] == b"PVRT", "cover extraction"
    assert items[1]["cover"] is None
    gd, nb = cli.list_ini(items)
    assert "02.name=SONIC ADVENTURE" in gd and "02.region=JUE" in gd and "03.product=HDR0016" in nb and "02.cover=1" in nb
    # CDI/ISO heuristic path: feed track03.iso as a plain .iso
    iso = os.path.join(sd, "02", "track03.iso")
    info = disc.read_cdi_or_iso(iso)
    assert info.ip.name == "SONIC ADVENTURE" and info.cover and info.cover[:4] == b"PVRT", "iso heuristic"
    # full build with a dummy menu binary
    menu_bin = os.path.join(sd, "menu.bin"); open(menu_bin, "wb").write(b"\x09\x00" * 1024)
    class A: pass
    a = A(); a.sd = sd; a.out = None; a.menu_bin = menu_bin; a.themes = os.path.join(ROOT, "themes"); a.covers = None; a.ip_template = None
    cli.cmd_build(a)
    out = os.path.join(sd, "01")
    assert sorted(os.listdir(out)) == ["disc.gdi", "track01.iso", "track02.raw", "track03.iso", "track04.raw", "track05.iso"], os.listdir(out)
    t5_lba = int(open(os.path.join(out, "disc.gdi")).read().splitlines()[-1].split()[1])
    assert t5_lba <= 544180 and t5_lba + os.path.getsize(os.path.join(out, "track05.iso")) // 2048 <= gdi.GD_END_LBA, t5_lba
    assert b"04.name=SHENMUE DISC 2" in gdi.read_hd_file(out, "LIST.INI")
    assert gdi.read_hd_file(out, "1ST_READ.BIN") == open(menu_bin, "rb").read()
    # background clip: encoded next to the game, appended raw to track05, indexed on track01
    if have_video:
        assert os.path.exists(os.path.join(sd, "02", "video.cvm"))
        from tools.cast_card import iso9660
        src1 = iso9660.FileSectorSource(os.path.join(out, "track01.iso"), 0)
        vidx = iso9660.read_file(src1, iso9660.find_file(src1, "VIDEOS.IDX")).decode().split()
        assert vidx[0] == "2" and vidx[4:6] == ["256", "256"] and int(vidx[2]) >= 10, vidx
        with open(os.path.join(out, "track05.iso"), "rb") as f:
            f.seek((int(vidx[1]) - t5_lba) * 2048); assert f.read(4) == b"CVID"
            f.seek((int(vidx[1]) - t5_lba + 1) * 2048); fr = f.read(2048 + 256 * 256 // 4); assert len(fr) == 18432 and fr[:8] != b"\0" * 8
    ip3 = open(os.path.join(out, "track03.iso"), "rb").read(0x8000); assert ip3[0x20:0x24] == b"%04X" % ipbin.crc16(ip3[0x40:0x50])
    menu = disc.read_gdi(os.path.join(out, "disc.gdi"))
    assert menu.ip.name == "CASTMENU", menu.ip
    # read LIST.INI back from the built image
    from tools.cast_card import iso9660
    for track, lba, expect in (("track01.iso", 0, {"1ST_READ.BIN", "LIST.INI", "CASTMENU.INI", "COVERS", "THEMES"}),):
        src = iso9660.FileSectorSource(os.path.join(out, track), lba)
        e = iso9660.find_file(src, "LIST.INI"); assert e, "LIST.INI on " + track
        txt = iso9660.read_file(src, e).decode("latin-1"); assert "04.name=SHENMUE DISC 2" in txt
        ext, size, _ = iso9660.root_dir(src)
        names = {x["name"].upper() for x in iso9660.list_dir(src, ext, size)}
        assert expect <= names, (track, names)
    gdi_txt = open(os.path.join(out, "disc.gdi")).read(); assert "2 " in gdi_txt and int(gdi_txt.splitlines()[2].split()[1]) > 150
    assert os.path.exists(os.path.join(sd, "GDEMU.ini"))
print("card tool OK")
