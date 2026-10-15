#include "screens.h"
#include "chrome.h"
#include "covers.h"
#include "anim.h"
#include "../settings.h"
#include <stdio.h>
#include <string.h>

static int sel; static game_t *items[8]; static int n;

void sidebar_open(void) {
    sel = 0; n = 0;
    const settings_t *s = settings_get();
    for (unsigned r = 0; r < sizeof s->recent / sizeof s->recent[0]; r++)
        for (int i = 0; i < g_list.count; i++)
            if (s->recent[r] && g_list.items[i].slot == s->recent[r] && !g_list.items[i].hidden) items[n++] = &g_list.items[i];
}
game_t *sidebar_game(void) { return n ? items[sel] : NULL; }

int sidebar_update(const input_t *in) {
    if (n) {
        if (in->repeat & BTN_UP) sel = (sel + n - 1) % n;
        if (in->repeat & BTN_DOWN) sel = (sel + 1) % n;
        if (in->pressed & BTN_A) return ACT_LAUNCH;
    }
    if (in->pressed & BTN_B) return ACT_CLOSE;
    return ACT_NONE;
}

void sidebar_draw(float base_alpha) {
    theme_t *t = &g_theme;
    float a = ease_out(ui_enter) * base_alpha;
    draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(120 * a)));
    float w = 300, x = -w + w * ease_out(ui_enter);
    /* vertical wordmark rail */
    draw_rect_grad_h(x, 0, 48, SCREEN_H, ALPHA(col_darken(t->bg, 0.4f), 240), ALPHA(col_darken(t->bg, 0.4f), 200));
    draw_rect(x + 47, 0, 1, SCREEN_H, ALPHA(t->accent, 120));
    draw_logo(x + 8, SCREEN_H - 48, 32, (int)(255 * a));
    /* panel */
    float px = x + 56, pw = w - 56;
    draw_shadow(px, 10, pw, SCREEN_H - 20, 16, ALPHA(0x000000, (int)(160 * a)));
    draw_rect_round_grad(px, 10, pw, SCREEN_H - 20, 12, col_lighten(ALPHA(t->panel, 255), 0.06f), col_darken(ALPHA(t->panel, 255), 0.12f));
    draw_rect_round_outline(px, 10, pw, SCREEN_H - 20, 12, ALPHA(t->accent, 160));
    const settings_t *s = settings_get();
    draw_avatar(px + 16, 26, 44);
    draw_text(&g_font, px + 70, 30, t->text_hi, s->nick);
    char lc[32]; snprintf(lc, sizeof lc, "%lu %s", (unsigned long)s->launches, tr(S_GAMES_LOWER));
    draw_text(&g_font_small, px + 70, 50, t->text_dim, lc);
    hairline(px + 16, 82, pw - 32);
    draw_text(&g_font_big, px + 16, 92, t->text_hi, tr(S_RECENT));
    if (!n) draw_text(&g_font_small, px + 16, 130, t->text_dim, tr(S_NEVER_PLAYED));
    for (int i = 0; i < n && i < 5; i++) {
        float y = 128 + i * 62; int is = i == sel;
        if (is) { draw_glow(px + pw / 2, y + 26, pw * 0.5f, col_scale_alpha(t->glow, 0.5f)); draw_rect_round_grad(px + 10, y, pw - 20, 56, 8, col_lighten(t->sel_bg, 0.15f), t->sel_bg); draw_rect_round_outline(px + 10, y, pw - 20, 56, 8, ALPHA(0xFFFFFF, 60)); }
        else draw_rect_round(px + 10, y, pw - 20, 56, 8, ALPHA(t->text_hi, 6));
        draw_cover(items[i], px + 18, y + 6, 44, 0);
        draw_text_clipped(&g_font, px + 72, y + 8, pw - 96, is ? t->sel_text : t->text, items[i]->name);
        char pt[24]; fmt_playtime(s->playtime[items[i]->slot < 1000 ? items[i]->slot : 0], pt, sizeof pt);
        draw_text(&g_font_small, px + 72, y + 32, is ? ALPHA(t->sel_text, 190) : t->text_dim, pt);
    }
    draw_hints(px + pw / 2, SCREEN_H - 28, tr(S_HINT_SIDEBAR), (int)(200 * a));
}
