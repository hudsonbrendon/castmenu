/* Themes: /cd/themes/<id>/theme.ini + bg.tex (+ optional icons). Built-in
 * fallback lives in the romdisk (/rd/themes/default). */
#pragma once
#include <stdint.h>
#include "draw.h"

#define THEME_MAX 16
#define THEME_ID_LEN 24

typedef struct {
    char id[THEME_ID_LEN];
    char name[32], author[32];
    uint32_t bg, panel, panel_alt, accent, accent_dim, text, text_dim, text_hi, sel_bg, sel_text, badge_j, badge_u, badge_e, badge_gd, badge_cd, shadow, accent2, glow;
    uint8_t  layout;          /* 0 = list, 1 = grid (default view) */
    uint8_t  panel_radius;    /* px */
    uint8_t  bg_dim;          /* 0..255 overlay darkness on bg image */
    uint8_t  cover_frame;     /* 1 draw frame around covers */
    uint8_t  font_scale;      /* 100 = 1.0 */
    uint8_t  orbs;            /* animated background orbs 0/1 */
    uint8_t  vignette;        /* 0..255 edge darkening */
    texture_t bg_tex;         /* optional */
    texture_t logo_tex;       /* optional */
    int loaded;
} theme_t;

extern theme_t g_theme;
extern font_t g_font, g_font_big, g_font_small, g_font_display;

int  theme_scan(void);                      /* fills theme_ids; returns count */
const char *theme_id(int i);
int  theme_count(void);
int  theme_load(int index);                 /* loads /cd/themes/<id>, falls back to romdisk */
void theme_unload(void);
uint32_t parse_color(const char *s);        /* #RRGGBB or #AARRGGBB */
