# Themes

A theme is a folder under `themes/<id>/` with:

```
theme.ini   colours + layout knobs (see below)
bg.png      background, any size (card tool resizes to 512×512, RGB565)
logo.png    optional 64×64 top-bar logo (ARGB4444)
```

`castmenu-card build` converts every theme folder into `themes/<id>/` on the
menu disc. Pick the theme in **Configurações → Tema**; the choice is saved to the VMU.
The built-in fallback (`default`) lives in the romdisk so the menu always boots.

## theme.ini

```ini
[theme]
name=Synthwave
author=you
bg=#12071F          ; used when bg.png is missing (gradient to panel_alt)
panel=#CC1C0B33     ; #AARRGGBB or #RRGGBB
panel_alt=#CC2A1250
accent=#FF2EAB
accent2=#2EE6FF     ; second glow color (orbs, swatches)
accent_dim=#7A1A5A
glow=#80FF2EAB      ; focus glow / background orbs (alpha matters)
text=#FDE7FF
text_dim=#B38BC9
text_hi=#FFFFFF
sel_bg=#FF2EAB      ; selected row background
sel_text=#12071F
badge_j=#FF2EAB     ; region badges
badge_u=#2EE6FF
badge_e=#FFD400
badge_gd=#FF8A00
badge_cd=#64748B
shadow=#80000000
layout=0            ; 0 list, 1 grid (initial view)
panel_radius=8      ; px
bg_dim=0            ; 0..255 dark overlay over bg.png
cover_frame=1
font_scale=100
orbs=1              ; drifting glow orbs on the background
vignette=90         ; 0..255 edge darkening
```

Nine themes ship in `themes/`: ocean (default, embedded), night, classic, cast-dark, cast-light,
dreamcast, synthwave, terminal, sega-blue. `tools/cast_card/themes.py` generates their backgrounds;
edit the PNGs freely.
