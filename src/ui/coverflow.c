#include "coverflow.h"
#include "chrome.h"
#include "covers.h"
#include "anim.h"
#include "../settings.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define CF_CY 235.0f       /* vertical center of the covers */
#define CF_SIZE 200.0f     /* center cover size */
#define CF_SIDE 140.0f     /* side cover size */
#define CF_GAP 118.0f      /* horizontal spacing between side covers */
#define CF_FLOOR (CF_CY + CF_SIZE / 2 + 2)

void coverflow_reset(int cursor) { (void)cursor; }

/* one cover at relative offset d (float, 0 = center). Returns nothing; draws cover + reflection. */
static void cover_at(const game_t *g, float d, int is_center) {
    theme_t *t = &g_theme;
    float ad = fabsf(d);
    if (ad > 3.2f) return;
    float side = clampf(ad, 0, 1);                       /* 0 center .. 1 fully a side cover */
    float size = CF_SIZE + (CF_SIDE - CF_SIZE) * side;
    float cx = SCREEN_W / 2 + (d < 0 ? -1 : 1) * (CF_SIZE / 2 + 30) * side + d * CF_GAP * (ad > 1 ? 1 : side) ;
    if (ad <= 1) cx = SCREEN_W / 2 + d * (CF_SIZE / 2 + CF_GAP * 0.55f + 30);
    else cx = SCREEN_W / 2 + (d < 0 ? -1 : 1) * (CF_SIZE / 2 + CF_GAP * 0.55f + 30 + (ad - 1) * CF_GAP);
    float skew = 0.18f * size * side * (d < 0 ? 1 : -1);  /* perspective: far edge shorter */
    float half = size / 2, cy = CF_FLOOR - half;          /* bottoms aligned on the floor */
    float x0 = cx - half, x1 = cx + half;
    float ytl = cy - half, ybl = cy + half, ytr = ytl, ybr = ybl;
    /* the edge away from the center recedes: shrink it vertically */
    if (d < 0) { ytl += skew; ybl -= skew; } else { ytr -= skew; ybr += skew; }
    float dim = 1 - 0.45f * side - 0.12f * clampf(ad - 1, 0, 2);
    int a = (int)(255 * dim);
    uint32_t tint = RGBA(a, a, a, 255);
    const texture_t *c = settings_get()->cover_mode ? cover_get(g->slot) : NULL;
    float pts[8] = { x0, ytl, x1, ytr, x0, ybl, x1, ybr };
    /* shadow / glow */
    if (is_center) { draw_glow(cx, cy, size * 0.95f, col_scale_alpha(t->glow, 0.9f)); draw_shadow(x0, ytl + 6, size, size, 16, ALPHA(0x000000, 150)); }
    else draw_shadow(x0 + 4, ytl + 6, size - 8, size - 8, 10, ALPHA(0x000000, (int)(110 * dim)));
    if (c) draw_tex_pts(c, pts, 0, 0, (float)c->iw / c->w, (float)c->ih / c->h, tint, tint, tint, tint);
    else {
        uint32_t c0 = col_lighten(t->accent_dim, 0.1f), c1 = col_darken(t->accent_dim, 0.35f);
        draw_rect_pts(pts, col_scale_alpha(ALPHA(c0, a), 1), col_scale_alpha(ALPHA(c0, a), 1), ALPHA(c1, 255), ALPHA(c1, 255));
        char ini[2] = { g->name[0] ? g->name[0] : '?', 0 };
        float sc = size / 80.0f;
        draw_text_scaled(&g_font_display, cx - text_width_scaled(&g_font_display, sc, ini) / 2, cy - g_font_display.line_h * sc / 2, sc, ALPHA(t->text_hi, (int)(70 * dim)), ini);
    }
    /* gloss + frame */
    float gl[8] = { x0, ytl, x1, ytr, x0, ytl + (ybl - ytl) * 0.4f, x1, ytr + (ybr - ytr) * 0.4f };
    draw_rect_pts(gl, ALPHA(0xFFFFFF, 30), ALPHA(0xFFFFFF, 30), ALPHA(0xFFFFFF, 0), ALPHA(0xFFFFFF, 0));
    uint32_t frame = is_center ? ALPHA(t->text_hi, 160) : ALPHA(t->text_hi, (int)(60 * dim));
    float e = 1.5f;
    float l[8] = { x0, ytl, x0 + e, ytl, x0, ybl, x0 + e, ybl }; draw_rect_pts(l, frame, frame, frame, frame);
    float r[8] = { x1 - e, ytr, x1, ytr, x1 - e, ybr, x1, ybr }; draw_rect_pts(r, frame, frame, frame, frame);
    float tp[8] = { x0, ytl, x1, ytr, x0, ytl + e, x1, ytr + e }; draw_rect_pts(tp, frame, frame, frame, frame);
    float bt[8] = { x0, ybl - e, x1, ybr - e, x0, ybl, x1, ybr }; draw_rect_pts(bt, frame, frame, frame, frame);
    /* reflection: mirrored vertically below the floor, fading */
    float rh = size * 0.42f;
    float rf[8] = { x0, ybl + 2, x1, ybr + 2, x0, ybl + 2 + rh * (ybl - ytl) / size, x1, ybr + 2 + rh * (ybr - ytr) / size };
    uint32_t rt = ALPHA(0xFFFFFF, is_center ? 90 : (int)(55 * dim)), rb = ALPHA(0xFFFFFF, 0);
    if (c) { float vmax = (float)c->ih / c->h; draw_tex_pts(c, rf, 0, vmax, (float)c->iw / c->w, vmax * (1 - rh / size), rt, rt, rb, rb); }
    else draw_rect_pts(rf, ALPHA(t->accent_dim, is_center ? 70 : 40), ALPHA(t->accent_dim, is_center ? 70 : 40), rb, rb);
}

void coverflow_draw(int cursor, float *pos) {
    theme_t *t = &g_theme;
    int n = g_list.view_count;
    *pos = lerp_to(*pos, (float)cursor, 12, ui_dt);
    if (fabsf(*pos - cursor) < 0.002f) *pos = cursor;
    /* floor: darker band with a soft horizon line */
    draw_rect_grad(0, CF_FLOOR - 40, SCREEN_W, 40, ALPHA(t->bg, 0), ALPHA(col_darken(t->bg, 0.3f), 110));
    draw_rect_grad(0, CF_FLOOR, SCREEN_W, 120, ALPHA(col_darken(t->bg, 0.35f), 170), ALPHA(col_darken(t->bg, 0.35f), 0));
    hairline(60, CF_FLOOR, SCREEN_W - 120);
    if (!n) { draw_text_center(&g_font, SCREEN_W / 2, CF_CY - 10, t->text_dim, tr(S_NO_GAMES)); return; }
    /* draw far-to-near: left side from far, right side from far, center last */
    int center = (int)floorf(*pos + 0.5f);
    for (int k = 3; k >= 1; k--) {
        int i = center - k; if (i >= 0 && i < n) cover_at(list_view_item(i), i - *pos, 0);
        i = center + k; if (i >= 0 && i < n) cover_at(list_view_item(i), i - *pos, 0);
    }
    if (center >= 0 && center < n) cover_at(list_view_item(center), center - *pos, fabsf(center - *pos) < 0.5f);
    /* title + meta */
    const game_t *g = list_view_item(cursor);
    if (g) {
        float ty = CF_FLOOR + 62;
        draw_text_shadow(&g_font_display, SCREEN_W / 2 - text_width(&g_font_display, g->name) / 2 * (text_width(&g_font_display, g->name) > 560 ? 0 : 1), ty, t->text_hi, ALPHA(0x000000, 120), text_width(&g_font_display, g->name) > 560 ? "" : g->name);
        if (text_width(&g_font_display, g->name) > 560) draw_text_center(&g_font_big, SCREEN_W / 2, ty + 10, t->text_hi, g->name);
        float bx = SCREEN_W / 2 - 60; (void)bx;
        /* badges centered under the title */
        float bw = strlen(g->region) * 22 + 34 + (g->vga ? 42 : 0) + (g->disc[0] ? 46 : 0);
        region_badges(SCREEN_W / 2 - bw / 2, ty + 50, g);
        if (g->favorite) draw_text(&g_font, SCREEN_W / 2 + bw / 2 + 6, ty + 48, 0xFFFFD166, "\x2a");
    }
}
