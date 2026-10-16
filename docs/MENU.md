# Using the menu

Controls, settings, themes, VMU saves, cheats and the online features.

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

## VMU saves and cloud backup

Settings → VMU saves lists every memory card: **X** copies the selected save to the other VMU, **A**
deletes it, **Y** uploads it to your Cast server, **START** shows the cloud list to restore from.
Backups are stored per nickname on the server (`server/cast_server.py`, SQLite). The console cannot
write to the GDEMU card, so there is no backup to SD. `CASTMENU.SYS` (the menu settings) can be backed
up the same way.

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

