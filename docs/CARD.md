# Preparing the SD card

Everything the card tool does, step by step. The short version is in the [README](../README.md).

## Requirements

| On the PC | Why |
|---|---|
| Python 3.10+ with `pip install pillow numpy` | card tool, cover and clip conversion |
| `mkisofs` (macOS: `brew install cdrtools`; Debian/Ubuntu: `apt install genisoimage`; Windows: cdrtools or `xorrisofs`) | builds the menu ISO tracks |
| `ffmpeg` (optional) | encodes `video.mp4` into the menu clip format |
| `yt-dlp` (optional) | downloads a gameplay clip per game with `fetch --videos` |

| On the console | |
|---|---|
| GDEMU with firmware **5.13.0 or newer** | the menu uses the GDEMU image-switch commands |
| SD card formatted as **FAT32** (any size the GDEMU supports) | |
| VMU in the controller (optional) | saves settings, profile, favorites and play time (`CASTMENU.SYS`, 1 block) |

## Preparing the SD card

GDEMU reads one image per numbered folder. Folder `01` is the menu, folders `02` to `999` are games.

```
/GDEMU.ini           created by the card tool if missing
/01/                 Cast Menu (written by `build`; do not edit by hand)
/02/disc.gdi         one game per folder, GDI (with its track files) or CDI
/02/cover.png        optional: your own cover (square, 256×256 or larger)
/02/synopsis.pt.txt  optional: your own synopsis for that language
/02/video.mp4        optional: a short gameplay video (any length, the tool trims it)
/02/music.mp3        optional: the game's theme (first 45 s are used)
/02/shot1.png        optional: screenshot (shot2.png = title screen)
/02/cheats.txt       optional: cheat codes for this game (see Cheats)
/02/collection.txt   optional: collection names, one per line
/03/game.cdi
/avatars/*.png       optional: profile pictures (64×64 are enough)
/music/*.mp3         optional: menu background tracks (played when the game has no theme)
/collections/*.txt   optional: one file per collection, lines = folder numbers, product ids or titles
/cheats/*.txt        optional: cheats by product id, e.g. cheats/MK51000.txt
/server.txt          optional: host:port of your Cast Chat server
/loader.bin          optional: GDMENU's loader, selectable in Settings → Loader
/themes/             optional: extra theme packs (same format as `themes/` in this repo)
```

Images can be GDI (the `.gdi` plus all `trackNN.bin/raw` files), CDI, or a plain ISO. Names of the
image files do not matter; the folder number does. Compressed archives must be unpacked first.

Adding games from the PC:

```bash
python3 -m tools.cast_card add  /Volumes/GDEMU "Sonic Adventure.gdi" "Crazy Taxi.cdi"   # copies into the next free folders
python3 -m tools.cast_card sort /Volumes/GDEMU            # renumbers the folders alphabetically (optional)
python3 -m tools.cast_card scan /Volumes/GDEMU            # lists what the tool sees: name, region, GD/CD, disc n/m, VGA, product, cover
```

You can also create the folders and copy the files by hand; `build` only needs the image inside.

## Covers, synopses and clips

Everything is optional and layered; the tool uses the first that exists:

| Cover | Synopsis | Background clip | Music |
|---|---|---|---|
| `cover.png` / `.jpg` in the game folder | `synopsis.<lang>.txt` in the game folder (`pt`, `en`, `es`, `de`, `fr`, `it`) | `video.cvm` already encoded | `music.cma` already encoded |
| `--covers DIR` with `<PRODUCT>.png` files (product id as on the disc, e.g. `MK51000.png`) | `synopsis.txt` (used for any language) | `video.mp4` / `.mkv` / `.webm` / `.mov` in the game folder | `music.mp3` / `.ogg` / `.wav` / `.m4a` / `.flac` in the game folder |
| `0GDTEX.PVR` inside the disc image | downloaded by `fetch` from Wikipedia (the menu language first, then English) | downloaded by `fetch --videos` (first YouTube hit for "<title> dreamcast gameplay") | downloaded by `fetch --music` (first YouTube hit for "<title> dreamcast ost main theme", 60 s) |
| libretro boxart downloaded by `fetch` | | | |
| a generated title card | | | |

Screenshots: `fetch` also downloads `shot1.png` (gameplay) and `shot2.png` (title screen) from libretro;
the details screen shows `shot1.png` when the game has no clip.

```bash
python3 -m tools.cast_card fetch /Volumes/GDEMU                 # covers + synopses that are missing
python3 -m tools.cast_card fetch /Volumes/GDEMU --videos        # also one clip per game
python3 -m tools.cast_card fetch /Volumes/GDEMU --force         # redo even what exists
python3 -m tools.cast_card fetch /Volumes/GDEMU --no-synopses   # covers only
python3 -m tools.cast_card fetch /Volumes/GDEMU --music         # also a theme / OST clip per game
python3 -m tools.cast_card build /Volumes/GDEMU --videos-redo   # re-encode every clip
python3 -m tools.cast_card build /Volumes/GDEMU --music-redo    # re-encode every music clip
```

`fetch` writes `cover.png`, `synopsis.<lang>.txt` and `video.mp4` next to each image, so you can
check or replace them before running `build`. Clips are trimmed to 12 s at 10 fps, 256×256, with
black bars cropped automatically; they live in the high-density area of the menu image and stream
straight from the GD-ROM without decoding on the console.

Music is trimmed to 45 s (menu tracks to 2 minutes), loudness-normalised, faded and stored as 22 kHz
mono ADPCM (about 500 KB per game). Menu tracks go in `/music` on the card root and play, one after the
other, whenever the selected game has no theme. Settings → Visual has switches for game music, menu
music and UI sounds.

Other per-game text files (one line each): `name.txt` (title shown in the menu), `genre.txt`,
`year.txt`, `players.txt`, `publisher.txt`, `category.txt` or `platform.txt` (puts the game in that
carousel; Atomiswave, CPS1 / CPS2, Neo Geo CD and Ports & Homebrew are also detected from the title).

### Collections and search

A collection is a carousel of your own, after the categories: create `collections/<Name>.txt` on the
card root with one game per line (folder number such as `02`, product id such as `MK51000`, or the exact
title), or put a `collection.txt` with the collection names in the game folder. Rebuild the card; the
collection appears under L / R like any category. In the menu, **L + R** opens the letter jump; **Y**
there opens the on-screen keyboard to search titles (also with a Dreamcast keyboard).

### Cheats

Put the codes in `cheats.txt` next to the image, or in `cheats/<PRODUCT>.txt` on the card root (product
id without dashes, as `scan` prints it). Text format, blocks separated by a blank line:

```
# Sonic Adventure (MK51000)
Infinite lives
00C1234A 00000009

Max rings
01C12350 000003E7
```

The first line of a block is the name, the following lines are `XXXXXXXX YYYYYYYY` pairs in the usual
CodeBreaker / GameShark text form. In the details screen press **X**, toggle the cheats with **A** and
leave with **B**; the choice is saved to the VMU (first 8 cheats of each game). The open loader installs
a tiny hook on the GD-ROM syscall that re-applies the enabled codes while the game runs. Only constant
writes are honoured: types `0` (8-bit), `1` (16-bit) and `2` (32-bit); conditional, pointer and slide
codes are ignored. Cheats need Settings → Loader → Cast (not GDMENU's loader). Experimental: some
games will not like it, turn the cheats off if a game stops booting.

## Building the card

```bash
python3 -m tools.cast_card build /Volumes/GDEMU
```

`build` scans every folder, reads each disc's `IP.BIN` (name, product, region, VGA, disc n/m), writes
`name.txt` and `serial.txt` next to each image, generates `LIST.INI` (GDMENU format) and
`CASTMENU.INI` (extra metadata), converts covers, synopses, avatars, themes and clips, and writes the
menu as a GDI in `01/`. It also creates `GDEMU.ini` with sensible defaults if there is none.

Options: `--menu-bin FILE` (another `1ST_READ.BIN`), `--covers DIR`, `--themes DIR`,
`--server host:port`, `--gdmenu-loader FILE`, `--videos-redo`, `--out DIR` (write the menu
somewhere else, e.g. to test in an emulator).

Run `build` again whenever you add, remove or rename games or change any of the extras. It only
rewrites `01/`; your game folders are untouched except for `name.txt` / `serial.txt` / `video.cvm`.

**macOS**: Finder leaves `._*` files on FAT cards; run `dot_clean -m /Volumes/GDEMU` before ejecting.
**Windows**: run the commands from PowerShell inside the unzipped release folder with `py -3 -m tools.cast_card ...`.

## Compatibility

- Cards made with **GDMENU Card Manager** work as they are: Cast Menu reads `LIST.INI`. Run `build`
  once to add covers, synopses and clips.
- **openMenu** cards: the game folders are the same; run `build` to replace `01`.
- GDEMU firmware 5.13.0+; older firmware shows a warning and cannot switch images.
- Tested on a real GDEMU console and in the redream emulator (everything except the launch itself,
  which needs the real BIOS and GDEMU).

## Troubleshooting

| Symptom | Fix |
|---|---|
| Card not recognised / GDEMU LED keeps blinking | FAT32 only; make sure `01/disc.gdi` exists; eject properly (`dot_clean -m` on macOS) |
| "Update GDEMU to 5.13.0 or higher" | flash newer GDEMU firmware |
| Game missing from the list | one image per folder; the folder must be a number; run `scan` to see the error |
| A game does not boot with the open loader | Settings → Loader → GDMENU (needs `loader.bin` on the card root) |
| Wrong title or cover | `name.txt` / `cover.png` in the game folder, then `build` |
| No synopsis for a game | add `synopsis.<lang>.txt`; Wikipedia has no article for everything |
| Menu text looks wrong after a rebuild from source | `./build.sh clean && ./build.sh` |

