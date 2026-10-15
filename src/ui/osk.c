#include "osk.h"
#include "chrome.h"
#include "anim.h"
#include <string.h>
#include <stdio.h>

static const char *ROWS[2][4] = {
    { "1234567890-_", "QWERTYUIOP@.", "ASDFGHJKL:'!", "ZXCVBNM,?/()" },
    { "1234567890+=", "qwertyuiop#&", "asdfghjkl;\"*", "zxcvbnm<>\\[]" },
};
#define COLS 12
static int active, shift, row, col; static char *buf; static int cap; static char title[48]; static float anim;

void osk_open(const char *t, char *b, int c) { strncpy(title, t, sizeof title - 1); buf = b; cap = c; active = 1; row = 1; col = 0; shift = 0; anim = 0; }
int osk_active(void) { return active; }

int osk_update(const input_t *in) {
    if (!active) return 0;
    anim = clampf(anim + ui_dt * 5, 0, 1);
    int len = strlen(buf);
    /* physical keyboard */
    if (in->kbd_char) {
        if (in->kbd_char == '\n' || in->kbd_char == '\r') { active = 0; return 1; }
        if (in->kbd_char == 8) { if (len) buf[len - 1] = 0; else { active = 0; return -1; } }
        else if (in->kbd_char == 27) { active = 0; return -1; }
        else if (in->kbd_char >= 32 && len < cap - 1) { buf[len] = in->kbd_char; buf[len + 1] = 0; }
        return 0;   /* a typed character must not double as a mapped button (y = cancel, space = send) */
    }
    if (in->repeat & BTN_UP) row = (row + 4) % 5;        /* row 4 = space bar */
    if (in->repeat & BTN_DOWN) row = (row + 1) % 5;
    if (row < 4) {
        if (in->repeat & BTN_LEFT) col = (col + COLS - 1) % COLS;
        if (in->repeat & BTN_RIGHT) col = (col + 1) % COLS;
    }
    if (in->pressed & BTN_A) {
        if (row == 4) { if (len < cap - 1) { buf[len] = ' '; buf[len + 1] = 0; } }
        else if (len < cap - 1) { buf[len] = ROWS[shift][row][col]; buf[len + 1] = 0; }
    }
    if (in->pressed & BTN_B) { if (len) buf[len - 1] = 0; else { active = 0; return -1; } }
    if (in->pressed & BTN_X) { if (len < cap - 1) { buf[len] = ' '; buf[len + 1] = 0; } }
    if (in->pressed & (BTN_L | BTN_R)) shift ^= 1;
    if (in->pressed & BTN_START) { active = 0; return 1; }
    if (in->pressed & BTN_Y) { active = 0; return -1; }
    return 0;
}

void osk_draw(void) {
    if (!active) return;
    theme_t *t = &g_theme;
    float a = ease_out(anim);
    float h = 212, y = SCREEN_H - h + 30 * (1 - a), x = 24, w = SCREEN_W - 48;
    draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(110 * a)));
    draw_shadow(x, y, w, h, 14, ALPHA(0x000000, (int)(160 * a)));
    draw_rect_round_grad(x, y, w, h, 12, col_lighten(ALPHA(t->panel, 255), 0.06f), col_darken(ALPHA(t->panel, 255), 0.15f));
    draw_rect_round_outline(x, y, w, h, 12, ALPHA(t->accent, 150));
    draw_text(&g_font_small, x + 16, y + 10, t->text_dim, title);
    char cnt[16]; snprintf(cnt, sizeof cnt, "%d/%d", (int)strlen(buf), cap - 1);
    draw_text_right(&g_font_small, x + w - 16, y + 10, ALPHA(t->text_dim, 150), cnt);
    draw_textbox(x + 16, y + 26, w - 32, 30, buf, "", 1);
    float kx = x + 16, ky = y + 66, kw = (w - 32 - 11 * 4) / COLS, kh = 22;
    for (int r = 0; r < 4; r++) for (int c = 0; c < COLS; c++) {
        float bx = kx + c * (kw + 4), by = ky + r * (kh + 4); int sel = r == row && c == col;
        if (sel) { draw_glow(bx + kw / 2, by + kh / 2, kw, col_scale_alpha(t->glow, 0.8f)); draw_rect_round_grad(bx, by, kw, kh, 5, col_lighten(t->sel_bg, 0.2f), t->sel_bg); }
        else { draw_rect_round(bx, by, kw, kh, 5, ALPHA(t->panel_alt, 230)); draw_rect_round_outline(bx, by, kw, kh, 5, ALPHA(t->text_dim, 40)); }
        char s[2] = { ROWS[shift][r][c], 0 };
        draw_text_center(&g_font, bx + kw / 2, by + 3, sel ? t->sel_text : t->text, s);
    }
    float sy = ky + 4 * (kh + 4); int ssel = row == 4;
    float sw = (kw + 4) * 8 - 4;
    if (ssel) draw_rect_round_grad(kx + (kw + 4) * 2, sy, sw, kh, 5, col_lighten(t->sel_bg, 0.2f), t->sel_bg);
    else { draw_rect_round(kx + (kw + 4) * 2, sy, sw, kh, 5, ALPHA(t->panel_alt, 230)); draw_rect_round_outline(kx + (kw + 4) * 2, sy, sw, kh, 5, ALPHA(t->text_dim, 40)); }
    draw_text_center(&g_font_small, kx + (kw + 4) * 2 + sw / 2, sy + 4, ssel ? t->sel_text : t->text_dim, tr(S_SPACE));
    /* shift indicator + send hint */
    draw_rect_round(kx, sy, (kw + 4) * 2 - 4, kh, 5, shift ? ALPHA(t->accent, 200) : ALPHA(t->panel_alt, 230));
    draw_text_center(&g_font_small, kx + kw + 2, sy + 4, shift ? t->sel_text : t->text_dim, "L/R abc");
    draw_rect_round(kx + (kw + 4) * 10, sy, (kw + 4) * 2 - 4, kh, 5, ALPHA(t->badge_e, 200));
    draw_text_center(&g_font_small, kx + (kw + 4) * 11 - 2, sy + 4, 0xFFFFFFFF, tr(S_SEND));
    draw_hints(SCREEN_W / 2, y + h - 12, tr(S_HINT_OSK), 200);
}
