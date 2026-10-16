# GDEMU ↔ menu protocol

Everything the menu needs from the GDEMU goes through one vendor ATAPI
packet opcode, `0x52`, sent on the G1 bus exactly like the KOS cdrom driver
sends `PACKET` commands. Observed in GDMENU 0.6 (`1ST_READ.BIN`, see
`REVERSE_ENGINEERING.md`) and matching megavolt85's `gdemu_sdk`.

| Packet bytes (12)                 | Meaning                                   | Data phase |
|-----------------------------------|-------------------------------------------|------------|
| `52 00 00 00 …`                   | Query version                             | 8 bytes in |
| `52 81 55 00 …`                   | Switch to next image                      | none       |
| `52 81 44 00 …`                   | Switch to previous image                  | none       |
| `52 82 LL HH …`                   | Switch to image `HH<<8 | LL` (0 = default)| none       |

Version reply (little-endian u32 pairs): `00 00 09 01` = bootloader 1.9
(zero on early boards), `00 00 14 05` = firmware 5.20. GDMENU refuses to run
below firmware 5.13.0 because `0x52 0x82` (direct slot switch) needs it.

After switching, the drive reports a disc change; KOS's `fs_iso9660` re-reads
the TOC on the next access, which is what the slow-scan path relies on.

## Files the menu reads from its own disc (`01/`)

| Path            | Writer                      | Purpose                                   |
|-----------------|-----------------------------|-------------------------------------------|
| `LIST.INI`      | GDMENU Card Manager / castmenu-card | `[GDMENU]` section, keys `NN.name/disc/vga/region/version/date` |
| `CASTMENU.INI`    | castmenu-card                 | `[CASTMENU]` extras: `product`, `gd`, `cover`, `genre`, `year`, `players`, `category`, `collection` (repeatable), `hidden` |
| `covers/NNN.tex`/`.pvr` | castmenu-card         | 128×128 cover (`.pvr` = original `0GDTEX.PVR` from the game disc) |
| `themes/<id>/`  | castmenu-card from `themes/`  | `theme.ini`, `bg.tex`, optional `logo.tex`, optional `move/select/back.wav` |
| `info/<lang>/NNN.txt` | castmenu-card           | synopsis per language (`any` = fallback) |
| `shots/NNN_1.tex`, `NNN_2.tex` | castmenu-card    | 256×192 screenshots (gameplay, title) |
| `cheats/NNN.txt` | castmenu-card              | cheat blocks (name + `XXXXXXXX YYYYYYYY` lines) |
| `videos.idx`    | castmenu-card                 | `slot lba frames fps w h`: CVM clips in the high-density area (track05) |
| `music.idx`     | castmenu-card                 | `slot lba bytes rate`: ADPCM music in the high-density area; slots ≥ 1000 are menu tracks |
| `avatars/*.tex` | castmenu-card                 | 64×64 profile pictures |

`NN` is the GDEMU folder number (`02`..`999`, 3 digits from 100 on). Slot 1 is
the menu itself and is hidden.

## Launch sequence

The menu plays a 3 s animation (`src/ui/launch_scene.c`: top view of a Dreamcast, lid opens,
the game's cover floats in as the disc label, seats and spins, progress bar) and fires the real
work behind it: settings save at 0.35 s (screen is black), image switch at 2.2 s, loader at 3.0 s.

1. `gdemu_img_set(slot)` → wait ≥ 600 ms (the animation covers it).
2. Write `loader_params_t` (8 × u32) at `0x8CCFFF00`.
3. Copy `cast_ldr.bin` to `0x8CE00000`, flush caches, jump through the P2 alias.
4. Loader restores the BIOS RAM image, patches it and re-enters the BIOS boot
   routine at `0x8C00B500`; the BIOS then boots the newly selected image.
