"""CMA: Cast Menu audio. A music clip as Yamaha AICA ADPCM (4 bit, mono) that the menu streams
straight from GD-ROM sectors into the sound chip (no decoding on the SH-4).

Layout: b'CAUD', u32 rate, u32 adpcm bytes, u16 channels (1), padded to one 2048-byte sector;
then the ADPCM bytes (two samples per byte, low nibble first, one dummy sample at the start as
the AICA decoder expects). Encoder ported from KallistiOS utils/wav2adpcm (public domain, superctr)."""
import os, struct, subprocess
import numpy as np

STEP_TABLE = (230, 230, 230, 230, 307, 409, 512, 614)
MUSIC_EXTS = (".mp3", ".ogg", ".wav", ".m4a", ".flac", ".aac", ".opus", ".wma")

def _clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v

def pcm2adpcm(samples):
    """samples: iterable of int16 -> bytes (AICA ADPCM)."""
    step_size, history = 127, 0
    nibbles = [0]                           # dummy first sample (the decoder reads the low nibble first)
    for s in samples:
        step = (int(s) & -8) - history
        a = (abs(step) << 16) // (step_size << 14)
        a = _clamp(a, 0, 7)
        if step < 0: a |= 8
        nibbles.append(a)
        # ymz_step: advance history / step size exactly like the chip
        delta = a & 7
        diff = _clamp(((1 + (delta << 1)) * step_size) >> 3, 0, 32767)
        history = _clamp(history - diff if a & 8 else history + diff, -32768, 32767)
        step_size = _clamp((STEP_TABLE[delta] * step_size) >> 8, 127, 24576)
    if len(nibbles) & 1: nibbles.append(0)
    out = bytearray(len(nibbles) // 2)
    for i in range(0, len(nibbles), 2): out[i // 2] = nibbles[i] | (nibbles[i + 1] << 4)
    return bytes(out)

def duration(path):
    try:
        out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path], capture_output=True, text=True, check=True).stdout
        return float(out.strip() or 0)
    except Exception: return 0.0

def pcm(path, start, seconds, rate):
    """ffmpeg -> int16 mono, loudness-normalised, with short fades so the loop does not click."""
    af = f"loudnorm=I=-16:LRA=11:TP=-1.5,afade=t=in:d=0.8,afade=t=out:st={max(0, seconds - 1.5)}:d=1.5"
    cmd = ["ffmpeg", "-v", "error", "-ss", str(start), "-i", path, "-t", str(seconds), "-vn", "-ac", "1", "-ar", str(rate),
           "-af", af, "-f", "s16le", "-acodec", "pcm_s16le", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    return np.frombuffer(raw[:len(raw) // 2 * 2], dtype="<i2")

def encode(src, dst, seconds=45, rate=22050, start=None):
    dur = duration(src)
    if start is None: start = 0 if dur <= seconds + 10 else min(8.0, dur - seconds)   # themes usually start at the top
    s = pcm(src, start, seconds, rate)
    if len(s) < rate: raise ValueError("no audio")
    data = pcm2adpcm(s.tolist())
    with open(dst, "wb") as f:
        f.write((b"CAUD" + struct.pack("<IIH", rate, len(data), 1)).ljust(2048, b"\0"))
        f.write(data); f.write(b"\0" * ((-len(data)) % 2048))
    return len(s) / rate

def find_music(folder):
    for fn in sorted(os.listdir(folder)):
        if fn.lower().startswith("music") and fn.lower().endswith(MUSIC_EXTS): return os.path.join(folder, fn)
    return None

if __name__ == "__main__":
    import sys, tempfile
    if len(sys.argv) > 2: print(encode(sys.argv[1], sys.argv[2]), "s"); sys.exit()
    # self-check: a 440 Hz tone survives encode -> decode (decoder mirrors the KOS reference)
    def adpcm2pcm(data):
        step_size, history, out, nib = 127, 0, [], 4
        i = 0
        while i < len(data):
            v = (data[i] >> (4 - nib)) & 15 if nib == 4 else (data[i] >> 4) & 15
            if nib == 4: v = data[i] & 15
            if not nib: i += 1
            nib ^= 4
            history = history * 254 // 256
            delta = v & 7; diff = _clamp(((1 + (delta << 1)) * step_size) >> 3, 0, 32767)
            history = _clamp(history - diff if v & 8 else history + diff, -32768, 32767)
            step_size = _clamp((STEP_TABLE[delta] * step_size) >> 8, 127, 24576)
            out.append(history)
        return np.array(out, dtype=np.int32)
    t = np.arange(22050) / 22050.0
    tone = (np.sin(2 * np.pi * 440 * t) * 12000).astype(np.int16)
    dec = adpcm2pcm(pcm2adpcm(tone.tolist()))[1:len(tone) + 1]
    err = np.abs(dec[2000:] - tone[2000:len(dec)]).mean()
    assert err < 1500, err
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "t.wav"); subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", src], check=True)
        secs = encode(src, os.path.join(d, "t.cma"), seconds=2)
        data = open(os.path.join(d, "t.cma"), "rb").read()
        assert data[:4] == b"CAUD" and struct.unpack("<IIH", data[4:14])[0] == 22050 and len(data) % 2048 == 0 and 1.9 < secs <= 2.01, (secs, len(data))
    print("audio OK")
