"""IP.BIN header parsing/patching (Dreamcast boot sector, 0x8000 bytes)."""
import struct
from dataclasses import dataclass

SIG = b"SEGA SEGAKATANA "

@dataclass
class IpBin:
    name: str; product: str; version: str; date: str; region: str; device: str
    disc: str; vga: bool; is_gd: bool; boot: str; company: str; periph: str

def parse(hdr: bytes) -> IpBin:
    if hdr[:16] != SIG: raise ValueError("not an IP.BIN")
    f = lambda o, n: hdr[o:o + n].decode("ascii", "replace").strip()
    periph = f(0x38, 8)
    import re as _re
    m = _re.match(r"[0-9A-Fa-f]+", periph); pval = int(m.group(0), 16) if m else 0
    area = hdr[0x30:0x38].decode("ascii", "replace")
    region = "".join(c for c in "JUE" if c in area[:3] and area["JUE".index(c)] == c)
    device = f(0x25, 6 + 5)
    disc = device[-3:] if "/" in device[-3:] else ""
    return IpBin(name=f(0x80, 64), product=f(0x40, 10), version=f(0x4A, 6), date=f(0x50, 8), region=region,
                 device=device, disc=disc, vga=bool(pval & 0x10), is_gd=device.startswith("GD-ROM"),
                 boot=f(0x60, 16), company=f(0x70, 16), periph=periph)

def patch(template: bytes, *, name="CASTMENU", product="T0000", version="V1.000", date="20261015",
          boot="1ST_READ.BIN", company="CASTMENU", region="JUE", vga=True) -> bytes:
    d = bytearray(template)
    def put(o, n, s): d[o:o + n] = s.encode("ascii").ljust(n)[:n]
    put(0x80, 64, name); put(0x40, 10, product); put(0x4A, 6, version); put(0x50, 8, date)
    put(0x60, 16, boot); put(0x70, 16, company)
    area = "".join(c if c in region else " " for c in "JUE") + "     "
    put(0x30, 8, area)
    put(0x25, 11, "GD-ROM1/1  ")
    import re as _re
    m = _re.match(r"[0-9A-Fa-f]+", d[0x38:0x40].decode("ascii", "replace")); periph = int(m.group(0), 16) if m else 0x0799A10
    periph = (periph | 0x10) if vga else (periph & ~0x10)
    put(0x38, 8, f"{periph:07X}")
    # region text blocks at 0x3700.. ("For JAPAN,TAIWAN,PHILIPINES." etc.) are left as-is: they are
    # only used by the BIOS when the area symbols above are set.
    return bytes(d)
