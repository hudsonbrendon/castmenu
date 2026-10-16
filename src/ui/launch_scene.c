#include "launch_scene.h"
#include "draw.h"
#include "theme.h"
#include "covers.h"
#include "chrome.h"
#include "lang.h"
#include "anim.h"
#include "../settings.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static texture_t body, lid, lid_in; static int loaded;
static const texture_t *cover; static float spin;

/* body/lid share a 256-space; the lid circle is centred at (128,122) r 90, hinge on its top edge */
#define CS 260.0f                       /* console size on screen */
#define CX ((SCREEN_W - CS) / 2)
#define CY 76.0f
#define K  (CS / 256)
#define TRAY_X (CX + 128 * K)
#define TRAY_Y (CY + 122 * K)
#define HINGE_V (32 / 256.0f)
#define LID_V1 (213 / 256.0f)

static float ease_io(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

void launch_scene_begin(const game_t *g) {
    if (!loaded) { texture_load("/rd/console/body.tex", &body); texture_load("/rd/console/lid.tex", &lid); texture_load("/rd/console/lid_in.tex", &lid_in); loaded = 1; }
    cover = settings_get()->cover_mode ? cover_get(g->slot) : NULL;
    spin = 0;
}

static void disc(float cx, float cy, float r, float alpha, float angle) {
    theme_t *t = &g_theme;
    uint32_t a = (uint32_t)(255 * alpha);
    if (cover) draw_tex_fan(cover, cx, cy, r, angle, ALPHA(0xFFFFFF, a));
    else {
        draw_circle(cx, cy, r, ALPHA(0xD9D9D6, a));
        if (t->logo_tex.ptr) draw_tex(&t->logo_tex, cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f, ALPHA(0xFFFFFF, a / 3));
    }
    draw_ring(cx, cy, r, ALPHA(0xFFFFFF, a / 2));             /* rim */
    draw_circle(cx, cy, r * 0.17f, ALPHA(0x2A2A2E, a));       /* hub ring: clear plastic */
    draw_circle(cx, cy, r * 0.11f, ALPHA(0x0A0A0C, a));       /* centre hole */
}

void launch_scene_draw(const game_t *g, float t, float dt) {
    theme_t *th = &g_theme;
    float ca = clampf((t - 0.35f) * 2.5f, 0, 1);
    if (ca <= 0) return;
    float zoom = 0.94f + 0.06f * ease_out(ca), cs = CS * zoom, cx = (SCREEN_W - cs) / 2, cy = CY + (CS - cs) / 2;
    float k = cs / 256, tx = cx + 128 * k, ty = cy + 122 * k;
    uint32_t white = ALPHA(0xFFFFFF, (int)(255 * ca));
    draw_glow(tx, ty, cs * 0.75f, col_scale_alpha(th->glow, 0.6f * ca));
    draw_tex(&body, cx, cy, cs, cs, white);

    /* lid: hinged on its back edge. The camera looks down from slightly in front, so the
     * lid foreshortens, passes edge-on near 50 degrees and then shows its underside
     * standing behind the console. */
    float th_a = 1.55f * ease_io((t - 0.8f) / 0.7f);          /* 0..89 degrees */
    float hy = cy + 32 * k, lh = (LID_V1 - HINGE_V) * cs;
    float proj = cosf(th_a) * 0.85f - sinf(th_a) * 0.53f;     /* screen-space height factor, camera tilt ~32 deg */
    float fy = hy + lh * proj, ex = cs * 0.14f * (fy - hy) / lh;
    float pts[8] = { cx, hy, cx + cs, hy, cx - ex, fy, cx + cs + ex, fy };
    if (proj >= 0) {
        uint32_t front = ALPHA(col_darken(0xFFFFFF, 0.5f * sinf(th_a)), (int)(255 * ca));
        draw_tex_pts(&lid, pts, 0, HINGE_V, 1, LID_V1, white, white, front, front);
    } else {
        uint32_t far = ALPHA(col_darken(0xFFFFFF, 0.25f), (int)(255 * ca));
        draw_tex_pts(&lid_in, pts, 0, HINGE_V, 1, LID_V1, white, white, far, far);
    }

    /* disc: floats down from above, lands on the spindle, then spins up */
    float td = clampf((t - 1.4f) / 0.8f, 0, 1), e = ease_out(td);
    float lift = 1 - e, dr = 80 * k * (1 + 0.45f * lift), dy = ty - 150 * lift, da = clampf(td * 3, 0, 1) * ca;
    if (t >= LAUNCH_T_MOUNT) spin += dt * 14.0f * clampf((t - LAUNCH_T_MOUNT) / 0.8f, 0.15f, 1);
    if (td > 0) {
        draw_glow(tx, ty + 6, dr * (0.9f + 0.5f * lift), ALPHA(0x000000, (int)(150 * e * ca)));  /* shadow in the tray */
        disc(tx, dy, dr, da, spin + 0.6f * lift);
        if (td >= 1) { draw_circle(tx, ty, 9 * k, ALPHA(0xA8A8A4, (int)(255 * ca))); draw_circle(tx, ty, 3 * k, ALPHA(0x606060, (int)(255 * ca))); }
    }

    /* caption + progress */
    float y = cy + cs + 12;
    draw_text_center(&g_font_big, SCREEN_W / 2, y, ALPHA(th->text_hi, (int)(255 * ca)), g->name);
    char dots[8]; int nd = ((int)(t * 3)) % 4; snprintf(dots, sizeof dots, "%.*s", nd, "...");
    char s2[48]; snprintf(s2, sizeof s2, "%s%s", tr(S_LAUNCHING), dots);
    draw_text_spaced(&g_font_small, SCREEN_W / 2 - text_width_spaced(&g_font_small, 2, tr(S_LAUNCHING)) / 2, y + 34, 2, ALPHA(th->text_dim, (int)(255 * ca)), s2);
    float bw = 260, bx = (SCREEN_W - bw) / 2, by = y + 58, p = clampf(t / LAUNCH_T_BOOT, 0, 1);
    draw_rect(bx, by, bw, 6, ALPHA(0xFFFFFF, (int)(40 * ca)));
    draw_rect_grad_h(bx, by, bw * p, 6, ALPHA(th->accent, (int)(255 * ca)), ALPHA(col_lighten(th->accent, 0.35f), (int)(255 * ca)));
    draw_rect(bx, by, bw * p, 2, ALPHA(0xFFFFFF, (int)(70 * ca)));
}
