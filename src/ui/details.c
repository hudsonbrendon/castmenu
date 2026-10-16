/* Game details: full screen. Cover + play time + facts on the left, the game's
 * clip in a window and the whole synopsis (scrollable) on the right. */
#include "screens.h"
#include "chrome.h"
#include "video.h"
#include "covers.h"
#include "anim.h"
#include "../settings.h"
#include <stdio.h>
#include <string.h>

#define SYN_X (SAFE_X + 200)   /* left panel: 184 + gap */
#define SYN_W (SCREEN_W - SAFE_X - SYN_X)
#define SYN_LH 19.0f
static game_t *cur; static char synopsis[4000]; static int has_syn; static float scroll, scroll_target; static int lines;

void details_open(game_t *g) {
    cur = g; scroll = scroll_target = 0;
    has_syn = list_synopsis(g, lang_codes[lang_current()], synopsis, sizeof synopsis) == 0 && synopsis[0];
    lines = draw_text_wrap(&g_font, 0, -1, SYN_W - 40, SYN_LH, 0, has_syn ? synopsis : tr(S_NO_SYNOPSIS));
}
game_t *details_game(void) { return cur; }

static float syn_h(void) { return BOTTOM_Y - 16 - CONTENT_Y - 52 - (video_has(cur->slot) ? 144 : 0); }

int details_update(const input_t *in) {
    float max = lines * SYN_LH - (syn_h() - 44); if (max < 0) max = 0;
    if (in->repeat & BTN_DOWN) scroll_target += SYN_LH * 2;
    if (in->repeat & BTN_UP) scroll_target -= SYN_LH * 2;
    if (in->repeat & BTN_R) scroll_target += SYN_LH * 7;
    if (in->repeat & BTN_L) scroll_target -= SYN_LH * 7;
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
    float dy = enter_dy();
    draw_topbar(tr(S_GAME_DETAILS), NULL); draw_topbar_status();
    /* left column: cover, play time, facts */
    float lx = SAFE_X + 12, ly = CONTENT_Y + dy + 12, lw = 160;
    panel(SAFE_X, CONTENT_Y + dy, lw + 24, BOTTOM_Y - 16 - CONTENT_Y);
    draw_cover(cur, lx, ly, 160, 0);
    char pt[24]; fmt_playtime(settings_get()->playtime[cur->slot < 1000 ? cur->slot : 0], pt, sizeof pt);
    float py = ly + 168;
    draw_rect_round_grad(lx, py, lw, 44, 8, col_lighten(ALPHA(t->accent, 255), 0.15f), ALPHA(t->accent, 255));
    draw_text_spaced(&g_font_small, lx + 12, py + 6, 1.5f, ALPHA(t->sel_text, 200), tr(S_PLAY_TIME));
    if (text_width(&g_font_big, pt) <= lw - 24) draw_text(&g_font_big, lx + 12, py + 18, t->sel_text, pt); else draw_text(&g_font, lx + 12, py + 22, t->sel_text, pt);
    char year[8], discs[4], players[8];
    snprintf(players, sizeof players, cur->players > 1 ? "1-%d" : "%d", cur->players ? cur->players : 1);
    snprintf(year, sizeof year, "%s", cur->year[0] ? cur->year : strlen(cur->date) == 8 ? (char[]){ cur->date[0], cur->date[1], cur->date[2], cur->date[3], 0 } : "-");
    snprintf(discs, sizeof discs, "%c", cur->disc[0] ? cur->disc[2] : '1');
    const struct { str_id k; const char *v; } rows[] = {
        { S_PUBLISHER, cur->publisher[0] ? cur->publisher : "-" }, { S_YEAR, year }, { S_PLAYERS, players },
        { S_DISCS, discs }, { S_GENRE, cur->genre[0] ? cur->genre : "-" },
    };
    float fy = py + 54;
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        float kw = text_width(&g_font_small, tr(rows[i].k));
        draw_text(&g_font_small, lx, fy, t->text_dim, tr(rows[i].k));
        draw_text_right_clipped(&g_font_small, lx + lw, fy, lw - kw - 10, t->text, rows[i].v);
        hairline(lx, fy + 15, lw); fy += 20;
    }
    /* right column: title, badges, clip window, synopsis */
    float rx = SYN_X, ry = CONTENT_Y + dy, rw = SYN_W;
    float bw = strlen(cur->region) * 22 + 34 + (cur->vga ? 42 : 0) + (cur->disc[0] ? 46 : 0);
    draw_text_clipped(&g_font_big, rx, ry - 6, rw, t->text_hi, cur->name);
    region_badges(rx + rw - bw, ry + 26, cur);
    if (cur->favorite) draw_text(&g_font, rx + rw - bw - 22, ry + 24, 0xFFFFD166, "\x2a");
    float sy = ry + 52;
    if (video_has(cur->slot)) {
        float vh = 136;
        draw_shadow(rx, sy, rw, vh, 10, t->shadow);
        draw_rect_round(rx, sy, rw, vh, 6, 0xFF000000);
        if (!video_draw_rect(rx + 2, sy + 2, rw - 4, vh - 4, 255)) {
            const texture_t *c = settings_get()->cover_mode ? cover_get(cur->slot) : NULL;
            if (c) draw_tex(c, rx + rw / 2 - (vh - 4) / 2, sy + 2, vh - 4, vh - 4, ALPHA(0xFFFFFF, 90));
            draw_text_center(&g_font_small, rx + rw / 2, sy + vh / 2 - 7, ALPHA(t->text_dim, 200), tr(S_LAUNCHING));
        }
        draw_rect_round_outline(rx, sy, rw, vh, 6, ALPHA(t->accent, 120));
        sy += vh + 8;
    }
    float sh = BOTTOM_Y - 16 - sy;
    panel(rx, sy, rw, sh);
    section_label(rx + 14, sy + 10, tr(S_SYNOPSIS));
    float cy = sy + 34, ch = sh - 44;
    draw_set_clip((int)rx + 4, (int)cy, (int)rw - 8, (int)ch);
    draw_text_wrap(&g_font, rx + 16, cy - scroll, rw - 40, SYN_LH, has_syn ? t->text : ALPHA(t->text_dim, 200), has_syn ? synopsis : tr(S_NO_SYNOPSIS));
    draw_clear_clip();
    draw_rect_grad(rx + 4, cy, rw - 8, 10, ALPHA(t->panel, 255), ALPHA(t->panel, 0));
    draw_rect_grad(rx + 4, cy + ch - 12, rw - 8, 12, ALPHA(t->panel, 0), ALPHA(t->panel, 255));
    float max = lines * SYN_LH - ch;
    if (max > 0) {
        float th = ch * ch / (lines * SYN_LH); if (th < 16) th = 16;
        draw_rect_round(rx + rw - 12, cy, 3, ch, 1.5f, ALPHA(t->text_dim, 40));
        draw_rect_round(rx + rw - 12, cy + (ch - th) * clampf(scroll / max, 0, 1), 3, th, 1.5f, t->accent);
    }
    draw_bottombar(tr(S_HINT_DETAILS));
}
