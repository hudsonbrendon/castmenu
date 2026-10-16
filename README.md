<p align="center"><img src="assets/brand/logo_sticker.png" width="320" alt="Cast Menu"></p>

<p align="center"><b>An open, good-looking game menu for GDEMU on the Sega Dreamcast.</b></p>

<p align="center">
<a href="https://github.com/hudsonbrendon/castmenu/actions/workflows/build.yml"><img src="https://github.com/hudsonbrendon/castmenu/actions/workflows/build.yml/badge.svg" alt="build"></a>
<a href="https://github.com/hudsonbrendon/castmenu/releases"><img src="https://img.shields.io/github/v/release/hudsonbrendon/castmenu" alt="release"></a>
<a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="MIT"></a>
</p>

Cast Menu replaces GDMENU on the SD card of a GDEMU-modded Dreamcast. It boots as folder `01`,
reads the games in the other folders and shows them in a coverflow with covers, gameplay clips,
synopses, play time and a Dreamcast-style launch animation. Everything is open source: the menu
(KallistiOS), the loader, the card tool and the optional chat server.

## Features

- **Coverflow, list and grid** views, smooth and animated, rendered on the PVR at 640×480.
- **A cover for every game**: taken from the disc (`0GDTEX.PVR`), from `cover.png` next to the image,
  downloaded from the libretro thumbnail server, or generated as a title card.
- **Background gameplay clips** behind the menu when you stop on a game (Batocera style), encoded
  by the card tool from `video.mp4` or downloaded with `yt-dlp`.
- **Details screen (Y)**: play time, publisher, year, players, discs, genre, a window looping the
  game's clip (or a screenshot), the full synopsis with scrolling, ◀ ▶ to move to the previous / next
  game, and **X** for the game's cheats.
- **Synopses** from Wikipedia in the menu language (pt, en, es, de, fr, it) or from your own text files;
  **screenshots** from the libretro thumbnail server.
- **Music**: each game's theme plays when you stop on it (`music.mp3` next to the image or
  `fetch --music`), menu background tracks from `/music` on the card, and short UI sounds (themes can
  ship their own). Streamed as AICA ADPCM straight from the GD-ROM.
- **Cheats** per game (`cheats.txt` in the game folder or `cheats/<PRODUCT>.txt` on the card), toggled
  from the details screen and applied by the open loader. Constant-write codes (8/16/32-bit). Experimental.
- **VMU saves**: list every memory card, copy a save to another VMU, delete, and back up / restore
  through the Cast server (the console cannot write to the GDEMU card itself).
- **Game ID** for VM2 / VMUPro / Pico2Maple: the product id is sent at launch so they switch virtual VMU
  on their own; the console clock is set from the VMU when unset and from the Cast server when online.
- **Smart categories**: All, Favorites, Last played, Atomiswave, CPS1 / CPS2, Neo Geo CD, Ports & Homebrew,
  plus your own **collections** (`collections/<Name>.txt` on the card) and a **search** box (L+R, then Y).
- **Launch animation**: a top-down Dreamcast opens its lid, the game's cover lands on the spindle
  as a disc and spins while a real progress bar follows the launch (settings saved → image mounted → boot).
- **Profile** with nickname and avatar, **last played** sidebar (B) with play time per game, favorites.
- **Open loader** with Region Free, Force VGA, In-Game Reset (A+B+X+Y+START), boot animation and
  SEGA license screen toggles; optional use of GDMENU's own `loader.bin`.
- **Themes** (Dreamcast, Dreamcast Dark, Controller, Night, Ocean, SEGA Blue, Synthwave, Terminal) in
  editable `theme.ini` + `bg.png` packs; **6 languages**; clock; screensaver.
- **System screen (START)**: GDEMU firmware, BIOS, region, cable, what is plugged into each
  controller port (controller name, VMU, rumble pack, microphone), total play time and the most played games.
- **Mouse and keyboard** navigation besides the controller.
- **Cast Chat** and **Cast AI** over the BBA/LAN adapter or a DreamPi modem: rooms, friends, private
  and voice messages (Dreamcast microphone), questions to an LLM by text or voice. Optional, needs
  the small Python server in `server/`.
- Works with cards prepared by **GDMENU Card Manager** (`LIST.INI`) and with the included
  **card tool**, which does everything from the PC.

## Screenshots

| | |
|---|---|
| ![coverflow](docs/screenshots/coverflow.png) | ![details](docs/screenshots/details.png) |
| ![launch animation](docs/screenshots/launch.png) | ![last played](docs/screenshots/sidebar.png) |
| ![list](docs/screenshots/list.png) | ![grid](docs/screenshots/grid.png) |
| ![settings](docs/screenshots/settings.png) | ![system](docs/screenshots/sysinfo.png) |
| ![theme Controller](docs/screenshots/theme-controller.png) | ![theme Dreamcast Dark](docs/screenshots/theme-dreamcast-dark.png) |

Captured in the redream emulator from a real card. More in `docs/screenshots/`.

## Install with an AI agent

Let an agent do the whole setup. Paste this into Claude Code, Codex, Cursor or any agent that can run
shell commands on the machine where the SD card is mounted:

```
Read https://raw.githubusercontent.com/hudsonbrendon/castmenu/main/docs/AI_INSTALL.md and follow it
to set up Cast Menu on my GDEMU SD card mounted at /Volumes/GDEMU. My game images are in ~/Dreamcast.
Fetch covers, synopses, clips and music, build the card, verify it and tell me when I can eject it.
```

The runbook in [docs/AI_INSTALL.md](docs/AI_INSTALL.md) tells the agent what to install, how to lay
out the card, what every command does and how to check the result.

## Quick start

1. Download `castmenu-<version>.zip` from [Releases](https://github.com/hudsonbrendon/castmenu/releases)
   and unzip it. It contains the menu binary (`1ST_READ.BIN`), the card tool (`tools/cast_card`),
   the themes and the docs. No compiler needed.
2. Install the PC requirements (see below): Python 3, Pillow, numpy, `mkisofs`; `ffmpeg` and
   `yt-dlp` only if you want clips.
3. Put your games on the SD card, one image per folder (`02`, `03`, ...). See *Preparing the SD card*.
4. From the unzipped folder, run:

   ```bash
   python3 -m tools.cast_card fetch /Volumes/GDEMU --videos --music   # covers + screenshots + synopses + clips + music (optional)
   python3 -m tools.cast_card build /Volumes/GDEMU            # writes the menu into 01/
   ```

   On Windows use the drive letter instead of `/Volumes/GDEMU` (for example `E:\`).
5. Eject the card, put it in the GDEMU, turn the Dreamcast on.

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

### VMU saves and cloud backup

Settings → VMU saves lists every memory card: **X** copies the selected save to the other VMU, **A**
deletes it, **Y** uploads it to your Cast server, **START** shows the cloud list to restore from.
Backups are stored per nickname on the server (`server/cast_server.py`, SQLite). The console cannot
write to the GDEMU card, so there is no backup to SD. `CASTMENU.SYS` (the menu settings) can be backed
up the same way.

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

## Using the menu

| Screen | Buttons |
|---|---|
| Library (coverflow / list / grid) | ◀ ▶ (▲ ▼) move · **A** launch · **Y** details · **B** last played · **X** settings · **START** system · **L / R** previous / next category · **L + R** jump to a letter (then **Y** search) |
| Details | ◀ ▶ previous / next game · ▲ ▼ scroll the synopsis · **L / R** page · **A** launch · **Y** favorite · **X** cheats · **B** back |
| VMU saves | **L / R** VMU · ▲ ▼ · **X** copy to the other VMU · **Y** upload · **START** cloud list · **A** delete · **B** back |
| Mouse | move left / right to browse, wheel to scroll, left button = A, right = B, side = X |
| Keyboard | arrows, Enter = A, Esc / Backspace = B, Tab = X, Y, Space = START, PgUp / PgDn = L / R; types into the search and chat |
| Last played (sidebar) | ▲ ▼ · **A** launch · **B** close |
| Settings | ▲ ▼ · **A** / ◀ ▶ change · **START** save to VMU · **B** back |
| Cast Chat | **X** write · **Y** hold to record voice · **L / R** rooms / friends · **A** open · **B** back |
| On-screen keyboard | **A** type · **B** delete · **X** space · **L / R** shift · **START** send · **Y** cancel |
| In game | **A + B + X + Y + START** returns to the menu (In-Game Reset) |

Multi-disc games show only disc 1 in the library; launching asks which disc to boot.

### Settings (X)

- **Profile**: nickname, avatar.
- **Loader**: Region Free, Force VGA, In-Game Reset, boot animation, SEGA license screen, loader
  (Cast or GDMENU's `loader.bin`), Game ID (VM2), VMU saves.
- **Visual**: theme, default view, language, background video, covers, game music, menu music, UI sounds.
- **Library**: sort order, region filter, hide extra discs.
- **Online**: Cast Chat, Cast AI, server, connect at boot.
- **System**: screensaver, GDEMU scan delay, save to VMU, reset to defaults, system info, exit to the console menu.

Settings are saved to the VMU (`CASTMENU.SYS`) with START; without a VMU the menu still works with defaults.

## Themes

A theme is a folder with `theme.ini` (colours, badges, layout knobs), `bg.png` (background) and an
optional `logo.png`. Edit the ones in `themes/`, or add folders to `/themes` on the card root, and run
`build`. Pick it in Settings → Theme. Format in [docs/THEMES.md](docs/THEMES.md).

## Cast Chat and Cast AI (optional)

`server/cast_server.py` is a small Python server (standard library only) for rooms, friends, private
and voice messages, VMU cloud backups, clock sync, a read-only Dreamcast Now counter, plus an LLM
bridge for Cast AI:

```bash
python3 server/cast_server.py --ai anthropic      # ANTHROPIC_API_KEY in the environment
```

Put `host:port` in `/server.txt` on the card (or Settings → Server). The Dreamcast connects through a
BBA / LAN adapter (DHCP) or the modem via DreamPi. Protocol and setup in [docs/CHAT.md](docs/CHAT.md)
and [docs/PROTOCOL.md](docs/PROTOCOL.md).

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

## Building from source

Docker + the `kazade/dreamcast-sdk` image (KallistiOS 2.3):

```bash
./build.sh            # assets → KOS build → 1ST_READ.BIN
./build.sh clean
python3 tests/test_card_tool.py
```

Details, emulator testing and CI in [docs/BUILD.md](docs/BUILD.md). The reverse-engineering notes that
made the loader possible are in [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md).

Layout: `src/` menu (KOS C), `src/net/` chat protocol, `ldr/` open loader, `server/` reference server,
`tools/cast_card/` card tool, `themes/` theme packs, `assets/lang/strings.tsv` translations, `docs/`.

## Roadmap

Open an issue to vote or to suggest more.

- Full cheat engine (conditional, pointer and slide codes) and an importer for the CodeBreaker database.
- Box-back art in the details screen.
- Dreamcast Now presence from the console (today only DreamPi reports it; the menu shows the online count).
- Theme editor on the console.

## Credits and license

MIT. Thanks to neuroacid (GDMENU), Deunan (GDEMU), KallistiOS, megavolt85 (gdemu_sdk), mrneo240
(openMenu), sonik-br (GDMENU Card Manager), the libretro thumbnail project and Wikipedia.
Dreamcast and the swirl are trademarks of SEGA; this is a fan project with no affiliation. The
Dreamcast logo artwork comes from Wikimedia Commons (public domain as a simple logo); the menu font
is the NiseSegaDreamcast fan font.
