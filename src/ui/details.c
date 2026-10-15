#include "screens.h"
#include "chrome.h"
#include "anim.h"
#include "../settings.h"
#include <stdio.h>
#include <string.h>

static game_t *cur; static char synopsis[2400]; static int has_syn; static float scroll, scroll_target; static int lines;

void details_open(game_t *g) {
    cur = g; scroll = scroll_target = 0;
    has_syn = list_synopsis(g, lang_codes[lang_current()], synopsis, sizeof synopsis) == 0 && synopsis[0];
    lines = draw_text_wrap(&g_font, 0, -1, 500, 19, 0, has_syn ? synopsis : tr(S_NO_SYNOPSIS));
}
game_t *details_game(void) { return cur; }

int details_update(const input_t *in) {
    float max = (lines - 7) * 19.0f; if (max < 0) max = 0;
    if (in->repeat & BTN_DOWN) scroll_target += 38;
    if (in->repeat & BTN_UP) scroll_target -= 38;
    if (in->repeat & BTN_R) scroll_target += 133;
    if (in->repeat & BTN_L) scroll_target -= 133;
    scroll_target = clampf(scroll_target, 0, max);
    scroll = lerp_to(scroll, scroll_target, 14, ui_dt);
    if (in->pressed & BTN_B) return ACT_CLOSE;
    if (in->pressed & BTN_A) return ACT_LAUNCH;
    if (in->pressed & BTN_Y) { list_toggle_favorite(cur); settings_save(); show_toast(cur->favorite ? tr(S_FAV_ADDED) : tr(S_FAV_REMOVED), 1); }
    return ACT_NONE;
}

void details_draw(void) {
    theme_t *t = &g_theme;
    if (!cur) return;
    float a = ease_out(ui_enter);
    draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(140 * a)));
    float w = 560, h = BOTTOM_Y - 10 - 50, x = (SCREEN_W - w) / 2, y = 50 + 14 * (1 - a);
    draw_shadow(x, y, w, h, 18, ALPHA(0x000000, (int)(170 * a)));
    draw_glow(x + w / 2, y, w * 0.6f, col_scale_alpha(t->glow, 0.5f * a));
    draw_rect_round_grad(x, y, w, h, 12, col_lighten(ALPHA(t->panel, 255), 0.06f), col_darken(ALPHA(t->panel, 255), 0.12f));
    draw_rect_round_outline(x, y, w, h, 12, ALPHA(t->accent, 170));
    draw_text_center(&g_font_big, SCREEN_W / 2, y + 12, t->accent, tr(S_GAME_DETAILS));
    /* header: cover + facts */
    draw_cover(cur, x + 20, y + 46, 104, 0);
    float fx = x + 142, fy = y + 44;
    float bw = strlen(cur->region) * 22 + 34 + (cur->vga ? 42 : 0) + (cur->disc[0] ? 46 : 0);
    draw_text_clipped(&g_font_big, fx, fy, w - 170 - bw - 10, t->text_hi, cur->name); fy += 30;
    char year[8], d[16], regions[48] = "", pt[24], discs[4];
    fmt_date(cur, d, sizeof d);
    snprintf(year, sizeof year, "%s", cur->year[0] ? cur->year : strlen(cur->date) == 8 ? (char[]){ cur->date[0], cur->date[1], cur->date[2], cur->date[3], 0 } : "-");
    for (const char *r = cur->region; *r; r++) { if (regions[0]) strcat(regions, " / "); strcat(regions, *r == 'J' ? tr(S_JAPAN) : *r == 'U' ? tr(S_USA) : tr(S_EUROPE)); }
    snprintf(discs, sizeof discs, "%c", cur->disc[0] ? cur->disc[2] : '1');
    fmt_playtime(settings_get()->playtime[cur->slot < 1000 ? cur->slot : 0], pt, sizeof pt);
    const struct { str_id k; const char *v; } rows[] = {
        { S_PUBLISHER, cur->publisher[0] ? cur->publisher : "-" }, { S_YEAR, year }, { S_RELEASED_IN, regions[0] ? regions : "-" },
        { S_DISCS, discs }, { S_GENRE, cur->genre[0] ? cur->genre : "-" }, { S_PLAY_TIME, pt },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        draw_text(&g_font_small, fx, fy + 2, t->text_dim, tr(rows[i].k));
        draw_text_clipped(&g_font_small, fx + 112, fy + 2, x + w - 20 - (fx + 112), t->text, rows[i].v);
        fy += 16;
    }
    region_badges(x + w - 20 - bw, y + 48, cur);
    if (cur->favorite) draw_text(&g_font, x + w - 34, y + 44, 0xFFFFD166, "\x2a");
    /* synopsis */
    float sy = fy + 10;
    hairline(x + 20, sy, w - 40);
    section_label(x + 20, sy + 8, tr(S_SYNOPSIS));
    float cy = sy + 30, ch = h - (cy - y) - 36;
    draw_set_clip((int)x + 16, (int)cy, (int)w - 32, (int)ch);
    draw_text_wrap(&g_font, x + 28, cy - scroll, 500, 19, has_syn ? t->text : ALPHA(t->text_dim, 200), has_syn ? synopsis : tr(S_NO_SYNOPSIS));
    draw_clear_clip();
    draw_rect_grad(x + 16, cy, w - 32, 10, ALPHA(t->panel, 255), ALPHA(t->panel, 0));
    draw_rect_grad(x + 16, cy + ch - 12, w - 32, 12, ALPHA(t->panel, 0), ALPHA(t->panel, 255));
    float max = (lines - 7) * 19.0f;
    if (max > 0) {
        float th = ch * ch / (lines * 19.0f); if (th < 16) th = 16;
        draw_rect_round(x + w - 14, cy, 3, ch, 1.5f, ALPHA(t->text_dim, 40));
        draw_rect_round(x + w - 14, cy + (ch - th) * clampf(scroll / max, 0, 1), 3, th, 1.5f, t->accent);
    }
    draw_hints(SCREEN_W / 2, y + h - 16, tr(S_HINT_DETAILS), 200);
}
