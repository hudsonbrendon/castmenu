# Building

Requirements on the host: Docker, Python 3 with Pillow, `mkisofs` (brew install cdrtools).

```bash
./build.sh                 # menu: assets -> KOS build in kazade/dreamcast-sdk -> 1ST_READ.BIN
python3 tests/test_card_tool.py
python3 tests/test_proto.py      # chat protocol client vs server/cast_server.py
cc -Isrc tests/test_list.c src/list.c src/ini.c src/ipbin.c src/settings.c -o /tmp/t && /tmp/t
```

Writing a card:

```bash
python3 -m tools.cast_card scan  /Volumes/GDEMU          # what is there
python3 -m tools.cast_card add   /Volumes/GDEMU game.gdi  # copy into next free folder
python3 -m tools.cast_card build /Volumes/GDEMU           # LIST.INI, covers, themes, 01/ image, GDEMU.ini
```

Per-game overrides live next to the image: `name.txt`, `cover.png`, `genre.txt`, `year.txt`,
`players.txt`, `publisher.txt`, `category.txt` (or `platform.txt`; Atomiswave / CPS1-CPS2 /
Neo Geo CD / Ports & Homebrew are also auto-detected from the title), `synopsis.txt` or
`synopsis.<pt|en|es|de|fr|it>.txt`. On the card root: `avatars/*.png` (profile pictures) and
`server.txt` (`host:port` of the Cast Chat server). A folder of `<PRODUCT>.png` files can be
passed with `--covers`. Translations: `assets/lang/strings.tsv` (UTF-8, one column per language),
compiled by `tools/cast_card/mklang.py` during `./build.sh`.

Layout of the repo:

```
src/            KOS menu (C)
src/loader/     bridge that hands over to the loader
ldr/            open bootloader (no OS), linked at 0x8CE00000
tools/cast_card/  PC tool (Python)
themes/         editable theme packs
assets/         fonts (Dreamcast.ttf = NiseSegaDreamcast, free fan font; Inter, OFL) + brand + generated romdisk
docs/           this folder; REVERSE_ENGINEERING.md has the evidence trail
```

## Testing without hardware (redream)

redream ships an HLE BIOS, so the menu boots from the built `01/disc.gdi`:

```bash
python3 -m tools.cast_card build /path/to/fake_sd   # tests/test_card_tool.py shows how to fabricate games
redream /path/to/fake_sd/01/disc.gdi
```

Set `port0=dev:2,desc:keyboard,type:keyboard` in `redream.cfg` to drive the menu with the host
keyboard (arrows, Enter = A, Backspace/Esc = B, Tab = X, Y = Y, Space = START, PgUp/PgDn = L/R).
Everything up to the launch screen runs in redream. The launch itself cannot: the loader re-enters the
real BIOS (`SR.BL=1`, jump to `0x8C00B500`), which the HLE BIOS does not implement, and there is no
GDEMU to switch images. Those two paths (loader, `0x52` vendor commands, slow scan) need a real
console + GDEMU ≥ 5.13.

## CI / releases

`.github/workflows/build.yml` builds the menu in the `kazade/dreamcast-sdk` container on every push,
runs the host tests and uploads `1ST_READ.BIN` + a zip as artifacts. Pushing a tag `vX.Y.Z` also
creates a GitHub Release with those files (bump `src/version.h` first).
