# GDMENU 0.6 — reverse-engineering notes

Target: `1ST_READ.BIN` from GDMENU v0.6.0 (neuroacid, 2016-08-24), as bundled by
GDMENU Card Manager (`tools/gdMenu/menu_data/1ST_READ.BIN`, SHA-256
`884eeeb6df2f00a2f989fbb61b0c4008d794dc183424f2ce8b5409f7c017103a`, 597 704 bytes,
unscrambled since it ships in GDI form).

Tooling: REA 6.4.0 CLI with the Ghidra 12.1.4 provider, language
`SuperH4:LE:32:default`. REA's ELF pre-check only admits x86/ARM/MIPS, so the raw
SH-4 image was wrapped in a minimal ELF tagged `EM_ARM` with the real language
forced through `REA_GHIDRA_LANGUAGE_ID`; Ghidra analysed it as SuperH4. The macOS
native decompiler (`os/mac_arm_64/decompile`) is not in the public zip and was
built from the shipped sources. REA evidence IDs are kept in
`scratch` snapshots (`gdmenu.rea`, `ldr3.rea`); the key ones are quoted below.

Terminology: **observed** = read directly from decompiled/disassembled code;
**inferred** = our interpretation; **unknown** = not resolved.

## 1. What the binary is

Observed from strings: KallistiOS 2.0 (`Copyright (C) 1997-2014 KallistiOS`),
Parallax (`plx_txr_*`), iniparser, libpng 1.5.9, libjpeg, an embedded romfs
(`-rom1fs-` at file offset 0x6B9B8, 83 424 bytes) holding:

| file           | size   | role                                   |
|----------------|--------|----------------------------------------|
| `loader.bin`   | 7 556  | bootloader (section 4)                 |
| `cb_loader.bin`| 43 516 | CodeBreaker variant of the loader      |
| `texture.png`  | 30 672 | UI sprite sheet                        |
| `font_8x16.png`| 1 429  | bitmap font                            |

UI strings: `OPTIONS`, `SYSINFO`, `REGION FREE`, `FORCE VGA`, `IN-GAME RESET`,
`BOOT INTRO`, `SEGA LICENSE`, `SAVE SETTINGS`, `GDEMU FIRMWARE`, `BOOTROM VERSION`,
`SYSTEM REGION`, `BROADCAST FORMAT`, `VIDEO CABLE`, `EXIT TO SYSTEM MENU?`,
`CODEBREAKER / SELECT A GAME TO LOAD WITH IT?`, `GDEMU FIRMWARE UPDATE 5.13.0 OR
HIGHER IS REQUIRED!`, `RETURNING TO SYSTEM MENU IN %02d`. Settings persist in a
VMU file `GDMENU.SYS` ("GDmenu System Data").

## 2. Game list (`FUN_8c0156e0`, evidence `search '/cd/list.ini'` → xref 0x8c0156f2)

Observed: `iniparser_load("/cd/list.ini")`. If it fails → `FUN_8c015aa0` (slow
scan, section 3). Otherwise for `i = 1..N` it queries
`GDMENU:%02d.name/.disc/.region/.vga/.date/.version` and fills a 0x49-byte record:

| offset | field   | notes                                         |
|--------|---------|-----------------------------------------------|
| 0x00   | name    | 40 chars; `GENERIC APPLICATION` when empty     |
| 0x28   | disc    | 4 chars (`1/2`)                               |
| 0x2C   | region  | up to 3 chars, validated against `JUE`        |
| 0x3A   | date    | 8 chars, shown as `%.4s/%.2s/%.2s`            |
| 0x42   | version | 7 chars                                       |

The loop stops at the first missing `NN.name`. This is the format GDMENU Card
Manager writes and what Cast Menu's `src/list.c` reads unchanged.

## 3. GDEMU commands

Observed in `backend/gdemu_sdk.c` of openMenu (megavolt85) and consistent with
the `gd_spi_dma_cmd` strings and 0x52 packets in GDMENU: vendor opcode `0x52`,
sub-commands `0x81` (step ±: 0x55 next / 0x44 prev) and `0x82` (set slot, u16
LE). The version query returns 8 bytes; GDMENU requires firmware ≥ 5.13.0.
Full table in `PROTOCOL.md`. Cast Menu's `src/gdemu.c` implements exactly this.

The slow scan (no `list.ini`) is inferred from GDMENU's strings `No Disc inserted`,
`TOC read error`, `Error locating data track`, `CD read error %d`, `Not a game disc`,
`UNSUPPORTED DISC / EMPTY FOLDER` and the readme's description: switch slot, re-init
the drive, read the TOC, read the IP.BIN sector of the last data track. Cast Menu does
the same in `scan_slots()`.

## 4. Launch path (`FUN_8c016120`, xref of `/rd/loader.bin`)

Observed:

```
f = fopen("/rd/loader.bin", "rb"); size = filelength(f);
fread(0x8CD00000, 1, size, f); fclose(f);
params.game_region = ...; params.disc_type = ...; params.need_game_fix = ...;
memcpy(0xACCFFF00, &params, 0x20);
arch_exec(0x8CD00000, size);          // KOS: copies image to 0x8C010000 and jumps
```

The 32-byte parameter block (8 × u32) is exactly openMenu's `ldr_params_t`:
`region_free, force_vga, IGR, boot_intro, sega_license, game_region (0 J/1 U/2 E),
disc_type (0 CD/1 GD), need_game_fix`.

## 5. loader.bin (open reimplementation: `ldr/`)

### 5.1 Entry chain (observed)

```
0x8C010000  stub: copy 0x756 longs from 0x8C01002C to 0xACE00000, jump there
0xACE00000  loader_main: CCR = 0x0909 (flush/enable cache), jmp 0x8CE00024 (P1 alias)
0x8CE00024  stage2: clear .bss (0x8CE01D58..0x8CE01DC4), sp = 0x8D000000, jmp main (0x8CE008A8)
```

So the payload is linked at **0x8CE00000**, lives in the top 2 MB of RAM and runs
uncached. Cast Menu links its loader at the same address and the menu writes it
there directly instead of going through `arch_exec`.

### 5.2 main (`FUN_8ce008a8`)

```
vid_set_mode(0x1001, 1)        // 640x480 RGB565, clears the framebuffer  (FUN_8ce01470)
maple_init()                   // MDAPRO=0x6155404F, MDTSEL=0, MSYS=0xC3500000, MDEN=1
clear_marked_region()          // if *(u32*)0x8C004004 == 0xDEADBEEF zero 0x8C004000..0x8C248000
restore_bios_ram()             // section 5.3
patch_bios()                   // section 5.4
media = (*(u8*)0x8C000072 & 7) == 2 ? 0 : 1
bios_enter(media)              // section 5.5
```

`0x8C000072` is a BIOS work-RAM byte (inferred: boot-media/disc-type flag); the
value only selects `r1` for the BIOS routine. Cast Menu reproduces the test verbatim.

### 5.3 restore_bios_ram (`FUN_8ce0023c`)

Observed: on stock ROMs copy ROM→RAM `0xA0000100→0x8C000100 (0x40 longs)` and
`0xA0008000→0x8C008000 (0x7E000 longs)`; on japanese_cake ≥1.032
`0xA0005100→0x8C000100 (0x900C0 longs)` and `0xA0000400→0x8C000400 (0x7FF00 longs)`.
Inferred: this recreates the RAM-resident BIOS image a cold boot produces, so the
boot routine can be re-entered with the GDEMU already pointing at the new image.

BootROM detection (`FUN_8ce000e0`, `FUN_8ce00118`): version text at ROM+0x7CB;
bytes 0x7CF/0x7D0 ≥ '3','2' ⇒ custom 1.032+; == '1','d' ⇒ retail 1.01d; otherwise
1.004 (JP launch units).

### 5.4 patch_bios (`FUN_8ce00368`) — exact store table

All stores are 16-bit SH-4 opcodes unless marked u32. `d` = ROM is 1.01d.

| condition                         | address → value |
|-----------------------------------|-----------------|
| IGR                               | `0xAC000198 ← 0xFF86` |
| force_vga, !d                     | `0x8C000536 ← 0x0009`, `0x8C000538 ← 0xE001` (nop; mov #1,r0) |
| force_vga, d                      | `0x8C00054C ← 0x0009`, `0x8C00054E ← 0xE001` |
| region_free                       | code cave 0x8C003CC0..0x8C003CFE (31 words, see `ldr/ldr.c: region_cave[]`), with `0x8C003CD2 ← 0xE230 + (region>>1)` and `0x8C003CD8 ← 0xE230 + region` (mov #'0'+n, r2); redirect `0x8C0004FA..0x8C00050E` (`D103 D203 [2212|0009 if need_game_fix] E100 [A00C|A016 if d] 6213 0009 3CC0 8C00 00B8 8C00`) |
| !boot_intro && disc_type==GD      | `0x8C00CAFA ← 0xE501`, `0x8C00CB0C ← 0xE500` |
| !sega_license && disc_type==GD    | `0x8C000424 ← D073`, `0x8C00044C ← D46A`, `0x8C00044E ← E307`, `0x8C000464 ← D5D1`; if !region_free `0x8C0004FA ← [A01A|A010 if d]`, `0x8C0004FC ← 0009`; `0x8C000590 ← C784` (`0x8C000592` if d); `0x8C0005BA ← 9319`; `0x8C0005D0.. ← 14BC 0009 024C A021 0E38 E200 143A E200 0242 E500 01A0 A03D`; `0x8C0005F0 ← 0100`, `0x8C0005F2 ← 0`; u32 `0x8C0005F4 ← [0x8C000AE4|0x8C000AD4 if d]`; u32 `0x8C0005F8 ← 0xAC008300`; `0x8C0007A4.. ← 4301 3044 3130` ("\1CD001"); u32 `0x8C0007AC ← 0x8CE00000`; `0x8C00086E ← E000` |

Custom bootROM path (observed): no RAM patches; instead syscall vector
`*(0x8C0000B0)(flags, 0, 1, 1)` with `flags = 0x30 | (!boot_intro) | (!sega_license << 1)`
only when disc_type==GD, else `0x30`.

Inferred meaning: the cave at 0x8C003CC0 replaces the BIOS region check and makes
the console region string match the game (the readme says exactly that). The
license-skip patch rewrites the IP.BIN hand-off so the BIOS jumps past the
`0x8C008300` license code; the "\1CD001" bytes and the pointer back to the loader
suggest the patched routine re-validates the disc and can re-enter the loader (IGR
return path). **Unknown**: the precise purpose of `0xFF86 @ 0x8C000198` and of the
0xDEADBEEF-marked clear; Cast Menu reproduces both verbatim.

### 5.5 bios_enter (`FUN_8ce00060`)

Observed register image before `jmp 0x8C00B500`:
`r15=0x8D000000 GBR=0x8C000000 VBR=0x8C000000 DBR=0x8C000010 SR=0x700000F0
FPSCR=0x00040001 PR=0x8CE00000 r1=media_flag`, all other GPRs zero, MAC/T cleared.
`0x8C00B500` is inside the ROM image copied in 5.3 (ROM offset 0xB500) — inferred
to be the BIOS "boot disc" routine that reads IP.BIN, runs its checks and
executes `1ST_READ.BIN`.

### 5.6 cb_loader.bin

Not reversed in this pass (43 KB; CodeBreaker on-the-fly patching). Cast Menu 0.1
does not ship CodeBreaker support.

## 6. What Cast Menu does differently

* Open loader (`ldr/ldr.c`, `ldr/start.S`) instead of the opaque blob; same
  parameter block, same patches, same BIOS re-entry.
* Reads GDMENU's `LIST.INI` plus `CASTMENU.INI` extras; covers come from the game
  discs' `0GDTEX.PVR` (extracted on the PC) instead of switching images at runtime.
* PVR-rendered UI with themes, grid/list views, favourites, recent, filters,
  multi-disc grouping, pt-BR/EN, screensaver, VMU settings (`CASTMENU.SYS`).

## 7. Verification status

| Component                              | Status |
|----------------------------------------|--------|
| LIST.INI parsing, sort/filter, IP.BIN  | host unit test (`tests/test_list.c`) + redream |
| Card tool (GDI/ISO readers, covers, image build) | `tests/test_card_tool.py` |
| Menu UI (coverflow, list, grid, categories, details, sidebar, settings, sysinfo, dialogs, OSK, VMU save) | redream 1.5 screenshots in `docs/screenshots/` |
| Cast Chat / Cast AI protocol + server | `tests/test_proto.py` (host client vs server); UI exercised offline in redream |
| Network bring-up (BBA/LAN DHCP, modem PPP via DreamPi), microphone (SIP), AICA voice playback | written against KOS 2.3 APIs; **untested on hardware** |
| Play-time tracking (RTC at launch, credited at next boot) | logic in `settings.c`; needs a real launch |
| PVR cover decode (twiddled RGB565 256²), `.tex` covers | redream |
| VQ-compressed covers                   | code path written from the PVR format spec, untested |
| GDEMU `0x52` commands, slow scan       | protocol matches openMenu's working SDK; untested here (no GDEMU) |
| Open loader (`ldr/`)                   | byte-for-byte reproduction of the observed patch table; **first hardware run (2026-10-15) boots the menu, game launch under investigation**; Settings → Loader can switch to GDMENU's own `loader.bin` lifted by the card tool |
| CodeBreaker (`cb_loader.bin`)          | not implemented |
