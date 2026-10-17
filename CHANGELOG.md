# Changelog

Release notes are taken from this file: each `## x.y.z` section becomes the body of the GitHub release
for tag `vx.y.z` (CI runs `tools/release_notes.py`), followed by the auto-generated commit list.

## 0.5.1 — 2026-10-17

- Fix: settings, favorites, play time and language were never restored at boot. The VMU file keeps a
  zero CRC, which KallistiOS rejects; the menu now finds its settings block by its magic number.
- Fix: A / B / X / Y hint glyphs back in the controller colours; region, GD, VGA and disc tags in console grey.
- System screen: the statistics line no longer overlaps the most-played list.
- README: English screenshots of a real card, demo video with sound, shorter page with docs/CARD.md and docs/MENU.md.

## 0.5.0 — 2026-10-17

- Music: the selected game's theme (`music.mp3` / `fetch --music`), menu background tracks (`/music`),
  UI sounds with per-theme overrides. Streamed as AICA ADPCM from the GD-ROM, encoded by the card tool.
- Cheats per game (`cheats.txt`, `cheats/<PRODUCT>.txt`): toggled in the details screen (X), applied by
  a GD-ROM syscall hook installed by the open loader. Constant-write codes only. Experimental.
- VMU saves screen: list, copy between VMUs, delete, cloud backup / restore through the Cast server.
- Game ID for VM2 / VMUPro / Pico2Maple at launch; console clock set from the VMU or the server.
- Collections (`collections/*.txt`) as extra carousels and title search from the letter screen.
- Screenshots from libretro in the details window when a game has no clip; most-played statistics in System.
- Mouse navigation. Dreamcast Now online counter in Cast Chat.
- Look: the logo red replaces orange in every theme; region / GD / VGA / disc badges are console grey.
- Project language is English everywhere; `docs/AI_INSTALL.md` runbook for agents; release notes from this file.

## 0.4.0 — 2026-10-16

- Launch animation redrawn from top-down references of the console: square shell, big lid with the
  real logo lock-up, grey front wedge, POWER / OPEN, plateau seams and side dots. The front lip bar is gone.
- Details screen: ◀ ▶ jump to the previous / next game without leaving the screen.
- Brand regenerated from the public-domain Dreamcast logo SVG: sticker logo, icons, wordmark, social preview.
- README rewritten in English as a full manual (install, card layout, covers / synopses / clips, build,
  controls, settings, themes, troubleshooting, roadmap).
- Screenshots retaken from a real card in redream.

## 0.3.2 — 2026-10-16

- Fix for patchy rendering on real hardware: PVR object-pointer-block overflow bins enabled.
- Full-screen details with play time, clip window and the whole synopsis; Wikipedia synopses in the card tool.
- System screen lists what is plugged into every controller port.
- Clip encoder crops baked-in black bars.

## 0.3.1 — 2026-10-16

- Launch animation: the Dreamcast lid opens, the game's cover lands as a disc and spins while a real
  progress bar follows the launch stages.

## 0.3.0 — 2026-10-15

- A cover for every game (disc art, cover.png, libretro, generated card) and background gameplay clips.
