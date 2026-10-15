"""Pull files out of a romfs (-rom1fs-) image embedded in a raw binary (KOS romdisk).
Used to lift GDMENU's loader.bin from the user's own GDMENU 1ST_READ.BIN / track file."""
import struct

def extract(data: bytes, want: str):
    off = data.find(b"-rom1fs-")
    if off < 0: return None
    rom = data[off:off + struct.unpack(">I", data[off + 8:off + 12])[0]]
    def walk(pos):
        while pos:
            nxt, spec, fsize, _ = struct.unpack(">IIII", rom[pos:pos + 16])
            name_end = rom.index(b"\0", pos + 16); name = rom[pos + 16:name_end].decode("latin-1")
            hdr_end = (name_end + 16) & ~15; typ = nxt & 7; nxt &= ~15
            if name not in (".", ".."):
                if typ == 1:
                    r = walk(spec)
                    if r is not None: return r
                elif typ == 2 and name.lower() == want.lower(): return rom[hdr_end:hdr_end + fsize]
            pos = nxt
        return None
    return walk((rom.index(b"\0", 16) + 16) & ~15)
