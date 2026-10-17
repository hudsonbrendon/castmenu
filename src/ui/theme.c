#include "theme.h"
#include "sfx.h"
#include "../ini.h"
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

theme_t g_theme;
font_t g_font, g_font_big, g_font_small, g_font_display;
static char ids[THEME_MAX][THEME_ID_LEN];
static int n_ids;

uint32_t parse_color(const char *s) {
    if (*s == '#') s++;
    uint32_t v = strtoul(s, NULL, 16);
    if (strlen(s) <= 6) v |= 0xFF000000;
    return v;
}

static void defaults(void) {
    theme_t *t = &g_theme;
    memset(t, 0, sizeof *t);
    strcpy(t->id, "default"); strcpy(t->name, "Dreamcast"); strcpy(t->author, "Cast Menu");
    t->bg = 0xFFF2F2EF; t->panel = 0xF0FFFFFF; t->panel_alt = 0xF0E4E4E0; t->accent = 0xFFCF3311; t->accent_dim = 0xFFF0C4B8;
    t->text = 0xFF26272A; t->text_dim = 0xFF66696D; t->text_hi = 0xFF141416; t->sel_bg = 0xFFCF3311; t->sel_text = 0xFFFFFFFF;
    t->badge_j = 0xFFE23D3D; t->badge_u = 0xFF2D6BD1; t->badge_e = 0xFF3DB55A; t->badge_gd = 0xFFF2C229; t->badge_cd = 0xFFD6D6D2; t->shadow = 0x30000000; t->accent2 = 0xFF2D6BD1; t->glow = 0x40CF3311;
    t->layout = 0; t->panel_radius = 8; t->bg_dim = 0; t->cover_frame = 1; t->font_scale = 100; t->orbs = 1; t->vignette = 16;
}

static int cb(void *u, const char *sec, const char *k, const char *v) {
    (void)u; theme_t *t = &g_theme;
    if (strcasecmp(sec, "theme") && strcasecmp(sec, "colors")) return 1;
#define C(n) else if (!strcasecmp(k, #n)) t->n = parse_color(v)
#define N(n) else if (!strcasecmp(k, #n)) t->n = atoi(v)
    if (!strcasecmp(k, "name")) { strncpy(t->name, v, sizeof t->name - 1); }
    else if (!strcasecmp(k, "author")) { strncpy(t->author, v, sizeof t->author - 1); }
    C(bg); C(panel); C(panel_alt); C(accent); C(accent_dim); C(text); C(text_dim); C(text_hi); C(sel_bg); C(sel_text);
    C(badge_j); C(badge_u); C(badge_e); C(badge_gd); C(badge_cd); C(shadow); C(accent2); C(glow);
    N(layout); N(panel_radius); N(bg_dim); N(cover_frame); N(font_scale); N(orbs); N(vignette);
#undef C
#undef N
    return 1;
}

int theme_scan(void) {
    n_ids = 0;
    strcpy(ids[n_ids++], "default");
    file_t d = fs_open("/cd/themes", O_RDONLY | O_DIR);
    if (d >= 0) {
        const dirent_t *e;
        while ((e = fs_readdir(d)) && n_ids < THEME_MAX) {
            if (e->name[0] == '.' || !strcasecmp(e->name, "default")) continue;
            strncpy(ids[n_ids], e->name, THEME_ID_LEN - 1); ids[n_ids][THEME_ID_LEN - 1] = 0;
            for (char *p = ids[n_ids]; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            n_ids++;
        }
        fs_close(d);
    }
    /* keep the order stable: default first, then alphabetical */
    for (int i = 2; i < n_ids; i++) for (int j = i; j > 1 && strcmp(ids[j - 1], ids[j]) > 0; j--) { char tmp[THEME_ID_LEN]; strcpy(tmp, ids[j]); strcpy(ids[j], ids[j - 1]); strcpy(ids[j - 1], tmp); }
    return n_ids;
}
const char *theme_id(int i) { return (i >= 0 && i < n_ids) ? ids[i] : "default"; }
int theme_count(void) { return n_ids; }

void theme_unload(void) { texture_free(&g_theme.bg_tex); texture_free(&g_theme.logo_tex); g_theme.loaded = 0; }

int theme_load(int index) {
    char path[128];
    theme_unload(); defaults();
    const char *id = theme_id(index);
    const char *root = strcmp(id, "default") ? "/cd/themes" : "/rd/themes";
    strncpy(g_theme.id, id, sizeof g_theme.id - 1);
    snprintf(path, sizeof path, "%s/%s/theme.ini", root, id);
    if (ini_parse_file(path, cb, NULL) < 0 && strcmp(id, "default")) { /* fall back */
        defaults(); root = "/rd/themes"; id = "default";
        snprintf(path, sizeof path, "%s/%s/theme.ini", root, id); ini_parse_file(path, cb, NULL);
    }
    snprintf(path, sizeof path, "%s/%s/bg.tex", root, id); texture_load(path, &g_theme.bg_tex);
    snprintf(path, sizeof path, "%s/%s/logo.tex", root, id); texture_load(path, &g_theme.logo_tex);
    snprintf(path, sizeof path, "%s/%s", root, id); sfx_theme(path);   /* optional move/select/back.wav in the theme folder */
    g_theme.loaded = 1;
    return 0;
}
