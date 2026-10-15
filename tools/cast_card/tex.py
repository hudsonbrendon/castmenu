"""NTEX writer: PNG/JPG -> raw PowerVR texture understood by src/ui/texture.c.

Layout: b'NTEX', u16 w, u16 h, u16 iw, u16 ih, u32 pvr_fmt, pixels (16bpp LE,
non-twiddled). w/h are rounded up to powers of two (8..1024); the image is
placed top-left and iw/ih carry the real size so UVs stay exact.
"""
import struct
from PIL import Image

PVR_TXRFMT_ARGB1555 = 0 << 27
PVR_TXRFMT_RGB565 = 1 << 27
PVR_TXRFMT_ARGB4444 = 2 << 27
PVR_TXRFMT_NONTWIDDLED = 1 << 26

def _pow2(n, lo=8, hi=1024):
    p = lo
    while p < n: p *= 2
    return min(p, hi)

def encode(img: Image.Image, fmt="auto", max_size=1024) -> bytes:
    img = img.convert("RGBA")
    if img.width > max_size or img.height > max_size:
        img.thumbnail((max_size, max_size), Image.LANCZOS)
    iw, ih = img.size
    w, h = _pow2(iw), _pow2(ih)
    has_alpha = img.getextrema()[3][0] < 255
    if fmt == "auto": fmt = "argb4444" if has_alpha else "rgb565"
    canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0)); canvas.paste(img, (0, 0))
    px = canvas.tobytes()
    out = bytearray()
    if fmt == "rgb565":
        code = PVR_TXRFMT_RGB565
        for i in range(0, len(px), 4):
            r, g, b = px[i], px[i+1], px[i+2]
            out += struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
    elif fmt == "argb4444":
        code = PVR_TXRFMT_ARGB4444
        for i in range(0, len(px), 4):
            r, g, b, a = px[i], px[i+1], px[i+2], px[i+3]
            out += struct.pack("<H", (a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4))
    elif fmt == "argb1555":
        code = PVR_TXRFMT_ARGB1555
        for i in range(0, len(px), 4):
            r, g, b, a = px[i], px[i+1], px[i+2], px[i+3]
            out += struct.pack("<H", (a >> 7) << 15 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3))
    else:
        raise ValueError(fmt)
    hdr = b"NTEX" + struct.pack("<HHHHI", w, h, iw, ih, code | PVR_TXRFMT_NONTWIDDLED)
    return hdr + bytes(out)

def convert(src, dst, fmt="auto", size=None):
    img = Image.open(src)
    if size: img = img.resize((size, size), Image.LANCZOS)
    with open(dst, "wb") as f: f.write(encode(img, fmt))

if __name__ == "__main__":
    import sys
    convert(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "auto")
