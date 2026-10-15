#include "chrome.h"
#include "covers.h"
#include "anim.h"
#include "../settings.h"
#include "../net/netif.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _arch_dreamcast
#include <kos.h>
#endif

float ui_t, ui_dt, ui_enter;
static char toast[96]; static float toast_t; static int toast_kind;
static texture_t avatar_tex; static char avatar_loaded[AVATAR_LEN];

float enter_dy(void) { return 12 * (1 - ease_out(ui_enter)); }

/* Glass panel: shadow, vertical gradient body, top highlight, hairline border. */
void panel(float x, float y, float w, float h) {
    theme_t *t = &g_theme;
    draw_shadow(x, y, w, h, 10, t->shadow);
    draw_rect_round_grad(x, y, w, h, t->panel_radius, col_lighten(t->panel, 0.06f), col_darken(t->panel, 0.12f));
    draw_rect_round_outline(x, y, w, h, t->panel_radius, ALPHA(t->accent, 70));
    draw_rect_grad_h(x + t->panel_radius, y + 1, w / 2 - t->panel_radius, 1, ALPHA(t->text_hi, 0), ALPHA(t->text_hi, 80));
    draw_rect_grad_h(x + w / 2, y + 1, w / 2 - t->panel_radius, 1, ALPHA(t->text_hi, 80), ALPHA(t->text_hi, 0));
}
void section_label(float x, float y, const char *s) {
    theme_t *t = &g_theme;
    draw_rect_round(x, y + 3, 3, 10, 1.5f, t->accent);
    draw_text_spaced(&g_font_small, x + 10, y, 1.5f, t->text_dim, s);
}
void hairline(float x, float y, float w) {
    draw_rect_grad_h(x, y, w / 2, 1, ALPHA(g_theme.accent, 0), ALPHA(g_theme.accent, 110));
    draw_rect_grad_h(x + w / 2, y, w / 2, 1, ALPHA(g_theme.accent, 110), ALPHA(g_theme.accent, 0));
}

/* Dreamcast button glyphs: A red, B blue, X yellow, Y green. */
uint32_t button_color(const char *b) {
    switch (b[0]) { case 'A': return 0xFFE0453F; case 'B': return 0xFF3B7BE0; case 'X': return 0xFFE8C13A; case 'Y': return 0xFF3FBF5A; default: return ALPHA(g_theme.text_dim, 255); }
}
float draw_button_glyph(float x, float cy, const char *b, int alpha) {
    int round_btn = strlen(b) == 1 && strchr("ABXY", b[0]);
    uint32_t c = ALPHA(button_color(b), alpha);
    if (round_btn) {
        draw_circle(x + 9, cy, 9, ALPHA(0x000000, alpha / 3));
        draw_circle(x + 8, cy - 1, 8, c);
        draw_rect_grad(x + 2, cy - 8, 12, 5, ALPHA(0xFFFFFF, alpha / 3), ALPHA(0xFFFFFF, 0));
        draw_text(&g_font_small, x + 8 - text_width(&g_font_small, b) / 2, cy - 8, ALPHA(0xFFFFFF, alpha), b);
        return 20;
    }
    float w = text_width(&g_font_small, b) + 10;
    draw_rect_round(x, cy - 8, w, 16, 5, ALPHA(0x000000, alpha / 3));
    draw_rect_round(x, cy - 9, w, 16, 5, ALPHA(g_theme.panel_alt, alpha));
    draw_rect_round_outline(x, cy - 9, w, 16, 5, ALPHA(g_theme.text_dim, alpha / 2));
    draw_text(&g_font_small, x + 5, cy - 8, ALPHA(g_theme.text, alpha), b);
    return w + 4;
}
/* "A PLAY|B BACK" -> glyph + label pairs, centered. */
void draw_hints(float cx, float cy, const char *hints, int alpha) {
    char buf[200]; strncpy(buf, hints, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *items[10]; int n = 0;
    for (char *p = strtok(buf, "|"); p && n < 10; p = strtok(NULL, "|")) items[n++] = p;
    float total = 0; float widths[10];
    for (int i = 0; i < n; i++) {
        char *sp = strchr(items[i], ' '); int bl = sp ? sp - items[i] : (int)strlen(items[i]);
        char b[12] = { 0 }; memcpy(b, items[i], bl > 11 ? 11 : bl);
        float gw = bl == 1 && strchr("ABXY", b[0]) ? 20 : text_width(&g_font_small, b) + 14;
        widths[i] = gw + 4 + text_width(&g_font_small, sp ? sp + 1 : "") + 12;
        total += widths[i];
    }
    float x = cx - total / 2;
    for (int i = 0; i < n; i++) {
        char *sp = strchr(items[i], ' '); char b[12] = { 0 };
        int bl = sp ? sp - items[i] : (int)strlen(items[i]); if (bl > 11) bl = 11;
        memcpy(b, items[i], bl);
        float gw = draw_button_glyph(x, cy, b, alpha);
        draw_text(&g_font_small, x + gw + 4, cy - 8, ALPHA(g_theme.text_dim, alpha), sp ? sp + 1 : "");
        x += widths[i];
    }
}

void draw_background(void) {
    theme_t *t = &g_theme;
    if (t->bg_tex.ptr) draw_tex(&t->bg_tex, 0, 0, SCREEN_W, SCREEN_H, 0xFFFFFFFF);
    else draw_rect_grad(0, 0, SCREEN_W, SCREEN_H, t->bg, ALPHA(t->panel_alt, 255));
}
/* translucent layer: light sweep, drifting orbs, vignette, optional dim */
void draw_background_tr(void) {
    theme_t *t = &g_theme;
    if (t->orbs) {
        float a = ui_t * 0.11f;
        draw_glow(140 + 90 * sinf(a), 380 + 40 * cosf(a * 1.3f), 210, col_scale_alpha(t->glow, 0.55f));
        draw_glow(540 + 60 * cosf(a * 0.8f), 110 + 50 * sinf(a * 0.6f), 170, col_scale_alpha(ALPHA(t->accent2, t->glow >> 24), 0.5f));
        draw_glow(330 + 120 * sinf(a * 0.5f), 250 + 80 * cosf(a * 0.9f), 120, col_scale_alpha(t->glow, 0.25f));
        /* slow diagonal light sweep */
        float sx = fmodf(ui_t * 28, SCREEN_W + 400) - 200;
        float pts[8] = { sx, 0, sx + 120, 0, sx - 140, SCREEN_H, sx - 20, SCREEN_H };
        draw_rect_pts(pts, ALPHA(0xFFFFFF, 0), ALPHA(0xFFFFFF, 10), ALPHA(0xFFFFFF, 0), ALPHA(0xFFFFFF, 10));
    }
    if (t->vignette) {
        uint32_t v = RGBA(0, 0, 0, t->vignette), v0 = RGBA(0, 0, 0, 0);
        draw_rect_grad(0, 0, SCREEN_W, 90, v, v0); draw_rect_grad(0, SCREEN_H - 110, SCREEN_W, 110, v0, v);
        draw_rect_grad_h(0, 0, 70, SCREEN_H, col_scale_alpha(v, 0.7f), v0); draw_rect_grad_h(SCREEN_W - 70, 0, 70, SCREEN_H, v0, col_scale_alpha(v, 0.7f));
    }
    if (t->bg_dim) draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, t->bg_dim));
}
void draw_logo(float x, float y, float size, int alpha) {
    theme_t *t = &g_theme;
    draw_glow(x + size / 2, y + size / 2, size * 1.1f, col_scale_alpha(t->glow, alpha / 255.0f));
    if (t->logo_tex.ptr) { float g = size * 0.12f; draw_tex(&t->logo_tex, x - g, y - g, size + 2 * g, size + 2 * g, ALPHA(0xFFFFFF, alpha)); return; }
    draw_rect_round_grad(x, y, size, size, size / 4, ALPHA(col_lighten(t->accent, 0.2f), alpha), ALPHA(t->accent, alpha));
    draw_ring(x + size / 2, y + size / 2, size * 0.3f, ALPHA(0xFFFFFF, alpha));
    draw_circle(x + size / 2, y + size / 2, size * 0.09f, ALPHA(0xFFFFFF, alpha));
}
void draw_wordmark(float x, float y, int alpha) {
    theme_t *t = &g_theme;
    draw_logo(x, y, 32, alpha);
    draw_text_shadow(&g_font_big, x + 42, y + 2, ALPHA(t->text_hi, alpha), ALPHA(0x000000, alpha / 3), "Cast");
    float w = text_width(&g_font_big, "Cast");
    draw_text_shadow(&g_font_big, x + 42 + w + 6, y + 2, ALPHA(t->accent, alpha), ALPHA(0x000000, alpha / 3), "Menu");
}
void draw_avatar(float x, float y, float size) {
    theme_t *t = &g_theme;
    settings_t *s = settings_get();
    if (strcmp(avatar_loaded, s->avatar)) {
        texture_free(&avatar_tex); strncpy(avatar_loaded, s->avatar, AVATAR_LEN - 1);
        if (s->avatar[0]) { char p[64]; snprintf(p, sizeof p, "/cd/avatars/%s.tex", s->avatar); texture_load(p, &avatar_tex); }
    }
    draw_shadow(x, y + 2, size, size, 5, t->shadow);
    if (avatar_tex.ptr) draw_tex(&avatar_tex, x, y, size, size, 0xFFFFFFFF);
    else {
        draw_rect_round_grad(x, y, size, size, 6, col_lighten(t->accent, 0.2f), t->accent_dim);
        char ini[2] = { s->nick[0] ? s->nick[0] : 'P', 0 }; if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] -= 32;
        draw_text_center(&g_font_big, x + size / 2, y + size / 2 - 13, t->text_hi, ini);
    }
    draw_rect_round_outline(x, y, size, size, 6, ALPHA(t->text_hi, 90));
}
void draw_topbar_status(void) {
    theme_t *t = &g_theme;
    /* clock */
    time_t now = time(NULL); struct tm *tm = localtime(&now); char clock[8];
    snprintf(clock, sizeof clock, "%02d:%02d", tm ? tm->tm_hour : 0, tm ? tm->tm_min : 0);
    float cw = text_width(&g_font_big, clock);
    draw_text_shadow(&g_font_big, SCREEN_W - 24 - cw, 18, t->text_hi, ALPHA(0x000000, 90), clock);
    /* signal bars */
    net_status_t ns = net_status();
    int bars = ns.state == NET_UP ? (ns.ping_ms < 0 ? 4 : ns.ping_ms < 120 ? 4 : ns.ping_ms < 250 ? 3 : ns.ping_ms < 500 ? 2 : 1) : 0;
    float bx = SCREEN_W - 24 - cw - 40;
    for (int i = 0; i < 4; i++) {
        float h = 5 + i * 4;
        uint32_t c = i < bars ? (ns.state == NET_UP ? t->badge_e : t->text_dim) : ALPHA(t->text_dim, 70);
        if (ns.state == NET_CONNECTING) c = ALPHA(t->accent, 120 + (int)(100 * sinf(ui_t * 5 + i)));
        draw_rect_round(bx + i * 7, 40 - h, 5, h, 1.5f, c);
    }
    if (bars) draw_glow(bx + 12, 34, 18, col_scale_alpha(ALPHA(t->badge_e, 255), 0.35f));
}
void draw_topbar(const char *title, const char *subtitle) {
    theme_t *t = &g_theme;
    draw_rect_grad(0, 0, SCREEN_W, 64, ALPHA(t->bg, 200), ALPHA(t->bg, 0));
    draw_logo(24, 12, 32, 255);
    draw_text_spaced(&g_font_small, 66, 10, 2.0f, t->accent, "CAST MENU");
    draw_text_shadow(&g_font_big, 66, 22, t->text_hi, ALPHA(0x000000, 90), title);
    if (subtitle) draw_text(&g_font_small, 66 + text_width(&g_font_big, title) + 12, 30, t->text_dim, subtitle);
}
float chip(float rx, float y, uint32_t bg, uint32_t fg, const char *s) {
    float w = text_width(&g_font_small, s) + 14;
    draw_rect_round(rx - w, y, w, 18, 9, bg);
    draw_rect_round_outline(rx - w, y, w, 18, 9, ALPHA(fg, 50));
    draw_text(&g_font_small, rx - w + 7, y + 2, fg, s);
    return w + 6;
}
#define BOTTOM_Y 446.0f
void draw_bottombar(const char *hint) {
    theme_t *t = &g_theme;
    draw_rect_grad(0, BOTTOM_Y - 20, SCREEN_W, SCREEN_H - BOTTOM_Y + 20, ALPHA(t->bg, 0), ALPHA(t->bg, 230));
    hairline(24, BOTTOM_Y - 2, SCREEN_W - 48);
    if (toast_t <= 0) draw_hints(SCREEN_W / 2, BOTTOM_Y + 16, hint, 255);
}
void draw_badge(float x, float y, uint32_t bg, const char *txt) {
    float w = text_width(&g_font_small, txt) + 10;
    draw_rect_round(x, y + 1, w, 16, 4, ALPHA(0x000000, 60));
    draw_rect_round_grad(x, y, w, 16, 4, col_lighten(bg, 0.15f), bg);
    draw_text(&g_font_small, x + 5, y + 1, col_on(bg), txt);
}
float region_badges(float x, float y, const game_t *g) {
    for (const char *r = g->region; *r; r++) {
        char s[2] = { *r, 0 };
        draw_badge(x, y, *r == 'J' ? g_theme.badge_j : *r == 'U' ? g_theme.badge_u : g_theme.badge_e, s);
        x += 22;
    }
    draw_badge(x, y, g->is_gd ? g_theme.badge_gd : g_theme.badge_cd, g->is_gd ? "GD" : "CD"); x += 34;
    if (g->vga) { draw_badge(x, y, g_theme.accent_dim, "VGA"); x += 42; }
    if (g->disc[0]) { char d[8]; snprintf(d, sizeof d, "D%c/%c", g->disc[0], g->disc[2]); draw_badge(x, y, ALPHA(g_theme.panel_alt, 255), d); x += 46; }
    return x;
}
/* Cover with frame, shadow and (optional) reflection. Placeholder = gradient + initial. */
void draw_cover(const game_t *g, float x, float y, float size, int reflect) {
    theme_t *t = &g_theme;
    const texture_t *c = settings_get()->cover_mode ? cover_get(g->slot) : NULL;
    draw_shadow(x, y + 4, size, size, 8, t->shadow);
    if (c) draw_tex(c, x, y, size, size, 0xFFFFFFFF);
    else {
        draw_rect_grad4(x, y, size, size, col_lighten(t->accent_dim, 0.1f), t->accent_dim, col_darken(t->accent_dim, 0.3f), col_darken(t->panel, 0.2f));
        draw_glow(x + size * 0.75f, y + size * 0.25f, size * 0.6f, col_scale_alpha(t->glow, 0.5f));
        char ini[3] = { g->name[0] ? g->name[0] : '?', 0 };
        float sc = size / 72.0f;
        draw_text_scaled(&g_font_display, x + size / 2 - text_width_scaled(&g_font_display, sc, ini) / 2, y + size / 2 - g_font_display.line_h * sc / 2, sc, ALPHA(t->text_hi, 70), ini);
        if (size >= 96) draw_text(&g_font_small, x + 8, y + size - 20, ALPHA(t->text, 90), tr(S_NO_COVER));
    }
    draw_rect_grad(x, y, size, size * 0.45f, ALPHA(0xFFFFFF, 26), ALPHA(0xFFFFFF, 0));
    if (t->cover_frame) draw_rect_round_outline(x, y, size, size, 2, ALPHA(t->text_hi, 50));
    if (reflect) {
        float rh = size * 0.22f;
        if (c) draw_tex_reflect(c, x, y + size + 2, size, rh, ALPHA(0xFFFFFF, 110), ALPHA(0xFFFFFF, 0));
        else draw_rect_grad(x, y + size + 2, size, rh, ALPHA(t->accent_dim, 60), ALPHA(t->accent_dim, 0));
    }
}
void fmt_date(const game_t *g, char *out, int cap) {
    if (strlen(g->date) == 8) snprintf(out, cap, "%.2s/%.2s/%.4s", g->date + 6, g->date + 4, g->date); else snprintf(out, cap, "%s", g->date[0] ? g->date : "-");
}
void fmt_playtime(uint16_t minutes, char *out, int cap) {
    if (!minutes) { snprintf(out, cap, "%s", tr(S_NEVER_PLAYED)); return; }
    if (minutes < 60) snprintf(out, cap, "%d min", minutes);
    else snprintf(out, cap, "%d%s %02dmin", minutes / 60, tr(S_HOURS), minutes % 60);
}
/* word wrap; returns number of lines drawn (or that would be drawn if y < 0) */
int draw_text_wrap(const font_t *f, float x, float y, float w, float lh, uint32_t col, const char *s) {
    char line[128]; int lines = 0; const char *p = s;
    while (*p) {
        if (*p == '\n') { p++; lines++; continue; }
        int n = 0; int last_sp = -1;
        while (p[n] && p[n] != '\n' && n < 127) {
            line[n] = p[n]; line[n + 1] = 0;
            if (p[n] == ' ') last_sp = n;
            if (text_width(f, line) > w) { if (last_sp > 0) { line[last_sp] = 0; n = last_sp + 1; } else { line[n] = 0; } break; }
            n++;
        }
        if ((!p[n] || p[n] == '\n') && text_width(f, line) <= w) line[n] = 0;
        if (y >= 0) draw_text(f, x, y + lines * lh, col, line);
        lines++; p += n; while (*p == ' ') p++;
        if (lines > 60) break;
    }
    return lines;
}
void toggle_switch(float x, float y, int on, int alpha) {
    theme_t *t = &g_theme;
    uint32_t track = on ? t->accent : ALPHA(t->panel_alt, 255);
    draw_rect_round(x, y, 34, 18, 9, ALPHA(track, alpha));
    if (!on) draw_rect_round_outline(x, y, 34, 18, 9, ALPHA(t->text_dim, alpha / 2));
    if (on) draw_glow(x + 17, y + 9, 22, col_scale_alpha(t->glow, alpha / 255.0f * 0.8f));
    draw_circle(x + (on ? 25 : 9), y + 9, 6.5f, ALPHA(0xFFFFFF, alpha));
}
void pill_button(float x, float y, float w, int sel, const char *label) {
    theme_t *t = &g_theme;
    if (sel) { draw_glow(x + w / 2, y + 15, w * 0.6f, col_scale_alpha(t->glow, 0.8f)); draw_rect_round_grad(x, y, w, 30, 15, col_lighten(t->sel_bg, 0.2f), t->sel_bg); }
    else { draw_rect_round(x, y, w, 30, 15, ALPHA(t->panel_alt, 255)); draw_rect_round_outline(x, y, w, 30, 15, ALPHA(t->text_dim, 80)); }
    draw_text_center(&g_font, x + w / 2, y + 6, sel ? t->sel_text : t->text, label);
}
void dialog_box(const char *title, float w, float h) {
    theme_t *t = &g_theme;
    float a = ease_out(ui_enter);
    draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(150 * a)));
    float x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + 16 * (1 - a);
    draw_shadow(x, y, w, h, 18, ALPHA(0x000000, (int)(170 * a)));
    draw_glow(x + w / 2, y, w * 0.7f, col_scale_alpha(t->glow, 0.6f * a));
    draw_rect_round_grad(x, y, w, h, t->panel_radius + 2, col_lighten(ALPHA(t->panel, 255), 0.08f), col_darken(ALPHA(t->panel, 255), 0.1f));
    draw_rect_round_outline(x, y, w, h, t->panel_radius + 2, ALPHA(t->accent, 160));
    draw_rect_grad_h(x + 12, y, w / 2 - 12, 2, ALPHA(t->accent, 0), t->accent);
    draw_rect_grad_h(x + w / 2, y, w / 2 - 12, 2, t->accent, ALPHA(t->accent, 0));
    draw_text_center(&g_font, SCREEN_W / 2, y + 18, t->text_hi, title);
}
void show_toast(const char *s, int kind) { strncpy(toast, s, sizeof toast - 1); toast_t = 2.2f; toast_kind = kind; }
int toast_active(void) { if (toast_t > 0) toast_t -= ui_dt; return toast_t > 0; }
void toast_draw(void) {
    theme_t *t = &g_theme;
    if (toast_t <= 0) return;
    float a = clampf(toast_t, 0, 1) * ease_out(clampf((2.2f - toast_t) * 5, 0, 1));
    uint32_t bg = toast_kind == 3 ? t->badge_j : toast_kind == 2 ? t->badge_e : t->accent;
    const char *icon = toast_kind == 1 ? "\x2a" : toast_kind == 2 ? "\xfc" : toast_kind == 3 ? "!" : "i";
    float w = text_width(&g_font, toast) + 56, x = (SCREEN_W - w) / 2, y = BOTTOM_Y + 2, h = 30;
    draw_rect_round_grad(x, y, w, h, 15, ALPHA(col_lighten(bg, 0.15f), (int)(245 * a)), ALPHA(bg, (int)(245 * a)));
    draw_rect_round_outline(x, y, w, h, 15, ALPHA(0xFFFFFF, (int)(70 * a)));
    draw_circle(x + 17, y + 15, 10, ALPHA(0xFFFFFF, (int)(60 * a)));
    draw_text_center(&g_font_small, x + 17, y + 7, ALPHA(col_on(bg), (int)(255 * a)), icon);
    draw_text(&g_font, x + 34, y + 6, ALPHA(col_on(bg), (int)(255 * a)), toast);
}
void draw_textbox(float x, float y, float w, float h, const char *text, const char *placeholder, int focused) {
    theme_t *t = &g_theme;
    draw_rect_round(x, y, w, h, 8, ALPHA(col_darken(t->bg, 0.3f), 200));
    draw_rect_round_outline(x, y, w, h, 8, focused ? t->accent : ALPHA(t->accent, 90));
    if (focused) draw_glow(x + w / 2, y + h / 2, w * 0.5f, col_scale_alpha(t->glow, 0.25f));
    if (text && text[0]) {
        float tw = text_width(&g_font, text), ox = tw > w - 24 ? w - 24 - tw : 0;
        draw_set_clip((int)x + 6, (int)y, (int)w - 12, (int)h);
        draw_text(&g_font, x + 12 + ox, y + (h - g_font.line_h) / 2, t->text_hi, text);
        if (focused && fmodf(ui_t, 1.0f) < 0.5f) draw_rect(x + 13 + ox + tw, y + 6, 2, h - 12, t->accent);
        draw_clear_clip();
    } else {
        draw_text(&g_font, x + 12, y + (h - g_font.line_h) / 2, ALPHA(t->text_dim, 160), placeholder ? placeholder : "");
        if (focused && fmodf(ui_t, 1.0f) < 0.5f) draw_rect(x + 12, y + 6, 2, h - 12, t->accent);
    }
}
