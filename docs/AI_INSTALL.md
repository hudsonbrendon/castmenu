# Cast Menu: runbook for AI agents

You are an agent setting up Cast Menu (an open GDEMU menu for the Sega Dreamcast) on the user's SD
card. Work on the user's machine, run the commands yourself, verify every step, and stop to ask only
when something below says so. Everything here is also in README.md; this file is the short, ordered
version.

## 0. What you need from the user

- Where the SD card is mounted (macOS `/Volumes/<name>`, Linux `/media/<user>/<name>` or `/mnt/...`,
  Windows a drive letter). If unsure, list mounted volumes and ask.
- Where the game images are (GDI folders, CDI or ISO files). If the games are already on the card in
  numbered folders, skip step 3.
- Whether to download extras from the internet (covers, synopses, gameplay clips, theme music). Default: yes.

## 1. Requirements

Check, then install what is missing (ask before installing system packages on Linux/Windows):

```bash
python3 --version            # 3.10 or newer
python3 -m pip install --user pillow numpy
mkisofs -version || genisoimage -version || xorrisofs -version   # one of them
ffmpeg -version              # optional: clips and music
yt-dlp --version             # optional: downloading clips and music
```

macOS: `brew install cdrtools ffmpeg yt-dlp`. Debian/Ubuntu: `sudo apt install genisoimage ffmpeg` and
`pipx install yt-dlp`. Windows: install Python 3 from python.org, cdrtools (mkisofs) and ffmpeg, then
use `py -3` instead of `python3`.

## 2. Get the release

```bash
mkdir -p ~/castmenu && cd ~/castmenu
curl -sL https://api.github.com/repos/hudsonbrendon/castmenu/releases/latest \
  | python3 -c "import json,sys; [print(a['browser_download_url']) for a in json.load(sys.stdin)['assets'] if a['name'].endswith('.zip')]" \
  | xargs curl -sLO
unzip -oq castmenu-*.zip && cd castmenu-*/
ls 1ST_READ.BIN tools/cast_card   # both must exist
```

Run every `python3 -m tools.cast_card ...` command below from this folder.

## 3. Put the games on the card

The card must be FAT32. Folder `01` is the menu; games go in `02`, `03`, ... one image per folder.

```bash
python3 -m tools.cast_card add /Volumes/GDEMU /path/to/game1.gdi /path/to/game2.cdi   # copies into the next free folders
python3 -m tools.cast_card scan /Volumes/GDEMU                                        # every game must be listed without "!!"
```

`add` copies a `.gdi` together with its track files. Archives (zip/7z/rar) must be extracted first.
If `scan` prints `!!` for a folder, open it: the image is missing, misnamed or unreadable. Fix or remove it.

## 4. Extras (optional, needs internet)

```bash
python3 -m tools.cast_card fetch /Volumes/GDEMU --videos --music
```

This writes `cover.png`, `synopsis.<lang>.txt`, `video.mp4` and `music.mp3` next to each game.
It takes a few minutes for a full card. Lines starting with `??` are games with no match online;
that is fine, the menu shows a generated cover and no synopsis for them. The user can drop their own
`cover.png` / `synopsis.en.txt` / `video.mp4` / `music.mp3` into a game folder at any time.

## 5. Build the menu

```bash
python3 -m tools.cast_card build /Volumes/GDEMU
```

Expected output ends with `menu written to /Volumes/GDEMU/01`. It converts covers, synopses, clips,
music and themes and writes the menu image into `01/`, plus `GDEMU.ini` if there was none.

## 6. Verify

```bash
ls -la /Volumes/GDEMU/01         # disc.gdi, track01.iso, track02.raw, track03.iso, track04.raw, track05.iso
cat /Volumes/GDEMU/01/disc.gdi   # 5 tracks; the last LBA must be below 548000
python3 - <<'PY'
import os; sd="/Volumes/GDEMU"
bad=[d for d in sorted(os.listdir(sd)) if d.isdigit() and int(d)>1 and not any(f.lower().endswith((".gdi",".cdi",".iso")) for f in os.listdir(os.path.join(sd,d)))]
print("folders without an image:", bad or "none")
PY
```

macOS only: `dot_clean -m /Volumes/GDEMU` (removes `._*` files), then `diskutil unmount /Volumes/GDEMU`
(`diskutil unmount force` if a security tool holds it). Linux: `sync && umount`. Windows: eject from the tray.

Tell the user: insert the card, power on; GDEMU firmware 5.13 or newer is required. Press **START**
in the menu to see firmware, BIOS and controllers; **Y** on a game for details; **X** for settings.

## 7. If something goes wrong

| Symptom | Do |
|---|---|
| `mkisofs/genisoimage not found` | install cdrtools / genisoimage (step 1) |
| `menu data too large for the low-density track` | too many covers/themes; remove extra theme folders from `/themes` on the card |
| GDEMU does not boot the card | FAT32? `01/disc.gdi` present? card ejected cleanly? |
| A game does not start | Settings → Loader → GDMENU if `loader.bin` is on the card root; otherwise report the game and region |
| `??` for most games in `fetch` | no internet or the libretro / Wikipedia hosts are unreachable; retry later |

Do not edit files inside `01/` by hand; always rerun `build`.
