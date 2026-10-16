"""CVM: Cast Menu background video. A short silent clip as a sequence of PVR VQ textures
(zero decode cost on the Dreamcast: each frame is DMA'd straight into texture RAM).

Layout: b'CVID', u16 w, u16 h, u16 frames, u16 fps, padded to one 2048-byte sector; then frames of
2048 (codebook: 256 x 4 RGB565 texels, twiddled order) + w*h/4 (indices, twiddled).
Encoded with a few Lloyd iterations per frame, seeded from the previous frame's codebook."""
import os, re, struct, subprocess, json
import numpy as np

def _twiddle_index(n):
    """Morton order for an n x n map (y in the even bits): returns linear index -> twiddled index."""
    idx = np.zeros(n * n, dtype=np.int64)
    y, x = np.divmod(np.arange(n * n), n)
    def spread(v):
        r = np.zeros_like(v)
        for b in range(int(np.log2(n))): r |= ((v >> b) & 1) << (2 * b)
        return r
    idx[:] = spread(y) | (spread(x) << 1)
    return idx

def _rgb565(a):
    a = a.astype(np.uint16)
    return (a[..., 0] >> 3) << 11 | (a[..., 1] >> 2) << 5 | (a[..., 2] >> 3)

def _blocks(frame):
    """frame (h, w, 3) uint8 -> (h/2*w/2, 12) float32 in codebook texel order (x,y),(x,y+1),(x+1,y),(x+1,y+1)."""
    h, w, _ = frame.shape
    f = frame.reshape(h // 2, 2, w // 2, 2, 3).transpose(0, 2, 3, 1, 4)   # by, bx, dx, dy, c
    return f.reshape(-1, 12).astype(np.float32)

def vq(blocks, book=None, iters=3, rng=None):
    rng = rng or np.random.default_rng(0)
    if book is None:
        book = blocks[rng.choice(len(blocks), 256, replace=len(blocks) < 256)].copy(); iters = max(iters, 6)
    bb = (blocks * blocks).sum(1)[:, None]
    for _ in range(iters):
        d = bb - 2 * blocks @ book.T + (book * book).sum(1)[None, :]
        idx = d.argmin(1)
        counts = np.bincount(idx, minlength=256)
        sums = np.stack([np.bincount(idx, weights=blocks[:, c], minlength=256) for c in range(12)], 1).astype(np.float32)  # np.add.at is ~50x slower
        nz = counts > 0
        book[nz] = sums[nz] / counts[nz][:, None]
        empty = np.flatnonzero(~nz)
        if len(empty): book[empty] = blocks[rng.choice(len(blocks), len(empty))]
    d = bb - 2 * blocks @ book.T + (book * book).sum(1)[None, :]
    return book, d.argmin(1).astype(np.uint8)

def duration(path):
    try:
        out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "json", path], capture_output=True, text=True, check=True).stdout
        return float(json.loads(out)["format"]["duration"])
    except Exception: return 0.0

def detect_crop(path, start, seconds=3):
    """ffmpeg cropdetect over a few seconds: strips letterbox/pillarbox bars baked into the source."""
    try:
        out = subprocess.run(["ffmpeg", "-v", "info", "-ss", str(start), "-t", str(seconds), "-i", path, "-vf", "cropdetect=24:8:0", "-f", "null", "-"],
                             capture_output=True, text=True).stderr
        m = re.findall(r"crop=(\d+):(\d+):(\d+):(\d+)", out)
        if not m: return None
        w, h, x, y = map(int, m[-1])
        return f"crop={w}:{h}:{x}:{y}," if w >= 64 and h >= 64 else None
    except Exception: return None

def frames(path, start, seconds, fps, size, crop=""):
    cmd = ["ffmpeg", "-v", "error", "-ss", str(start), "-i", path, "-t", str(seconds), "-an",
           "-vf", f"{crop}fps={fps},scale={size}:{size}:flags=lanczos", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    n = len(raw) // (size * size * 3)
    return np.frombuffer(raw[:n * size * size * 3], dtype=np.uint8).reshape(n, size, size, 3)

def encode(src, dst, seconds=12, fps=10, size=256, start=None, log=None):
    dur = duration(src)
    if start is None: start = 0 if dur <= 30 else max(0, min(dur * 0.35, dur - seconds))   # skip intros/menus of long gameplay videos
    fr = frames(src, start, seconds, fps, size, detect_crop(src, start) or "")
    if len(fr) == 0: raise ValueError("no frames")
    tw = _twiddle_index(size // 2)
    book = None; rng = np.random.default_rng(1)
    with open(dst, "wb") as f:
        f.write((b"CVID" + struct.pack("<HHHH", size, size, len(fr), fps)).ljust(2048, b"\0"))
        for i, frame in enumerate(fr):
            book, idx = vq(_blocks(frame), book, rng=rng)
            cb = _rgb565(np.clip(book.reshape(256, 4, 3) + 0.5, 0, 255)).astype("<u2")
            out = np.zeros(len(idx), dtype=np.uint8); out[tw] = idx
            f.write(cb.tobytes()); f.write(out.tobytes())
            if log and i % 20 == 0: log(f"    frame {i}/{len(fr)}")
    return len(fr)

VIDEO_EXTS = (".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v")

def find_video(folder):
    for fn in sorted(os.listdir(folder)):
        if fn.lower().startswith("video") and fn.lower().endswith(VIDEO_EXTS): return os.path.join(folder, fn)
    return None

if __name__ == "__main__":
    import sys, tempfile
    if len(sys.argv) > 2: print(encode(sys.argv[1], sys.argv[2], log=print), "frames"); sys.exit()
    # self-check: twiddle is a permutation, VQ of a flat colour is exact, synthetic clip round-trips
    tw = _twiddle_index(128); assert sorted(tw) == list(range(128 * 128)) and tw[1] == 2 and tw[128] == 1
    flat = np.full((16, 16, 3), (200, 40, 90), np.uint8); book, idx = vq(_blocks(flat))
    assert np.allclose(book[idx[0]], [200, 40, 90] * 4, atol=0.5)
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "t.mp4")
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=30", "-t", "2", "-pix_fmt", "yuv420p", src], check=True)
        n = encode(src, os.path.join(d, "t.cvm"), seconds=1, fps=10)
        data = open(os.path.join(d, "t.cvm"), "rb").read()
        assert data[:4] == b"CVID" and struct.unpack("<HHHH", data[4:12]) == (256, 256, n, 10) and len(data) == 2048 + n * (2048 + 256 * 256 // 4), (n, len(data))
        assert data[2048:2048 + 8] != b"\0" * 8      # frame 0 codebook starts on sector 1
    print("cvm OK")
