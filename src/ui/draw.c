#include "draw.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static float z;
static pvr_poly_hdr_t hdr_flat_op, hdr_flat_tr;
static texture_t white, tex_disc, tex_ring, tex_glow, tex_shadow;
static int cur_list;
static int clip_on; static int clip_x, clip_y, clip_w, clip_h;

#define SOFT 64 /* size of the generated soft textures */

/* Generated at boot: antialiased disc (corners, circles), ring (outlines), radial
 * glow (orbs, highlights) and a blurred rounded square (9-slice drop shadow). */
static void make_soft_textures(void) {
    static uint8_t a[SOFT * SOFT];
    float c = SOFT / 2.0f - 0.5f, R = SOFT / 2.0f;
    for (int y = 0; y < SOFT; y++) for (int x = 0; x < SOFT; x++) {
        float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c));
        float v = R - d; a[y * SOFT + x] = (uint8_t)(255 * (v < 0 ? 0 : v > 1 ? 1 : v));
    }
    texture_from_alpha(&tex_disc, a, SOFT, SOFT);
    for (int y = 0; y < SOFT; y++) for (int x = 0; x < SOFT; x++) {
        float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c));
        float v = 1.5f - fabsf(d - (R - 1.5f)); a[y * SOFT + x] = (uint8_t)(255 * (v < 0 ? 0 : v > 1 ? 1 : v));
    }
    texture_from_alpha(&tex_ring, a, SOFT, SOFT);
    for (int y = 0; y < SOFT; y++) for (int x = 0; x < SOFT; x++) {
        float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c)) / R;
        float v = d >= 1 ? 0 : (1 - d) * (1 - d); a[y * SOFT + x] = (uint8_t)(255 * v);
    }
    texture_from_alpha(&tex_glow, a, SOFT, SOFT);
    /* rounded box (radius 12) with 14px gaussian-ish falloff; 9-sliced with a 32px corner */
    for (int y = 0; y < SOFT; y++) for (int x = 0; x < SOFT; x++) {
        float bx = fabsf(x - c) - (R - 20), by = fabsf(y - c) - (R - 20);
        float ox = bx > 0 ? bx : 0, oy = by > 0 ? by : 0;
        float d = sqrtf(ox * ox + oy * oy); /* distance outside the box, 0 inside */
        float v = 1 - d / 20.0f; v = v < 0 ? 0 : v > 1 ? 1 : v; v = v * v * (3 - 2 * v);
        a[y * SOFT + x] = (uint8_t)(255 * v);
    }
    texture_from_alpha(&tex_shadow, a, SOFT, SOFT);
}

void draw_init(void) {
    /* opb_overflow_count: the UI stacks many small translucent quads per 32x32 tile
     * (shadows, rounded panels, glyphs); without overflow OPBs the hardware silently
     * drops whatever exceeds the per-tile bin (patchy panels, missing keys/hints on
     * real PVR; emulators do not enforce it). */
    pvr_init_params_t params = { { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 }, 512 * 1024, 0, 0, 0, 6, 0 };
    pvr_init(&params);
    texture_solid(&white);
    make_soft_textures();
    pvr_poly_cxt_t c;
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY); pvr_poly_compile(&hdr_flat_op, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY); pvr_poly_compile(&hdr_flat_tr, &c);
}

void draw_frame_begin(void) {
    pvr_wait_ready(); pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY); cur_list = PVR_LIST_OP_POLY; z = 1.0f;
}
void draw_frame_tr(void) { pvr_list_finish(); pvr_list_begin(PVR_LIST_TR_POLY); cur_list = PVR_LIST_TR_POLY; }
void draw_frame_end(void) { pvr_list_finish(); pvr_scene_finish(); }
float draw_z_next(void) { z += 0.02f; return z; }

/* ---------- colors ---------- */
uint32_t col_mix(uint32_t a, uint32_t b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    uint32_t r = 0;
    for (int s = 0; s < 32; s += 8) {
        int ca = (a >> s) & 255, cb = (b >> s) & 255;
        r |= (uint32_t)(ca + (cb - ca) * t) << s;
    }
    return r;
}
uint32_t col_lighten(uint32_t c, float t) { return col_mix(c, ALPHA(0xFFFFFF, c >> 24), t); }
uint32_t col_darken(uint32_t c, float t) { return col_mix(c, ALPHA(0x000000, c >> 24), t); }
int col_luma(uint32_t c) { return (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) / 1000; }
uint32_t col_on(uint32_t bg) { return col_luma(bg) > 150 ? 0xFF14161C : 0xFFFFFFFF; }
uint32_t col_scale_alpha(uint32_t c, float k) { float a = (c >> 24) * k; return ALPHA(c, (int)(a < 0 ? 0 : a > 255 ? 255 : a)); }

/* ---------- core quad ---------- */
static void quad4(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) {
    if (w <= 0 || h <= 0) return;
    if (clip_on) {
        float x1 = x + w, y1 = y + h, cx1 = clip_x + clip_w, cy1 = clip_y + clip_h;
        float nx = x < clip_x ? clip_x : x, ny = y < clip_y ? clip_y : y;
        float nx1 = x1 > cx1 ? cx1 : x1, ny1 = y1 > cy1 ? cy1 : y1;
        if (nx1 <= nx || ny1 <= ny) return;
        float du = u1 - u0, dv = v1 - v0;
        float fx0 = (nx - x) / w, fx1 = (x1 - nx1) / w, fy0 = (ny - y) / h, fy1 = (y1 - ny1) / h;
        u0 += du * fx0; u1 -= du * fx1; v0 += dv * fy0; v1 -= dv * fy1;
        /* colors: interpolate the gradient the same way */
        uint32_t ntl = col_mix(col_mix(tl, tr, fx0), col_mix(bl, br, fx0), fy0);
        uint32_t ntr = col_mix(col_mix(tl, tr, 1 - fx1), col_mix(bl, br, 1 - fx1), fy0);
        uint32_t nbl = col_mix(col_mix(tl, tr, fx0), col_mix(bl, br, fx0), 1 - fy1);
        uint32_t nbr = col_mix(col_mix(tl, tr, 1 - fx1), col_mix(bl, br, 1 - fx1), 1 - fy1);
        tl = ntl; tr = ntr; bl = nbl; br = nbr;
        x = nx; y = ny; w = nx1 - nx; h = ny1 - ny;
    }
    float zz = draw_z_next();
    pvr_vertex_t v;
    v.flags = PVR_CMD_VERTEX; v.z = zz; v.oargb = 0;
    v.x = x; v.y = y + h; v.u = u0; v.v = v1; v.argb = bl; pvr_prim(&v, sizeof v);
    v.x = x; v.y = y; v.u = u0; v.v = v0; v.argb = tl; pvr_prim(&v, sizeof v);
    v.x = x + w; v.y = y + h; v.u = u1; v.v = v1; v.argb = br; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + w; v.y = y; v.u = u1; v.v = v0; v.argb = tr; pvr_prim(&v, sizeof v);
}
static void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c0, uint32_t c1) {
    quad4(x, y, w, h, u0, v0, u1, v1, c0, c0, c1, c1);
}

static void bind_flat(void) { pvr_prim(cur_list == PVR_LIST_OP_POLY ? &hdr_flat_op : &hdr_flat_tr, sizeof(pvr_poly_hdr_t)); }
static void bind_tex(const texture_t *t) {
    pvr_poly_cxt_t c; pvr_poly_hdr_t h;
    pvr_poly_cxt_txr(&c, cur_list, t->fmt, t->w, t->h, t->ptr, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h, &c); pvr_prim(&h, sizeof h);
}

/* ---------- flat ---------- */
void draw_rect(float x, float y, float w, float h, uint32_t argb) { bind_flat(); quad(x, y, w, h, 0, 0, 1, 1, argb, argb); }
void draw_rect_grad(float x, float y, float w, float h, uint32_t top, uint32_t bottom) { bind_flat(); quad(x, y, w, h, 0, 0, 1, 1, top, bottom); }
void draw_rect_grad_h(float x, float y, float w, float h, uint32_t l, uint32_t r) { bind_flat(); quad4(x, y, w, h, 0, 0, 1, 1, l, r, l, r); }
void draw_rect_grad4(float x, float y, float w, float h, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) { bind_flat(); quad4(x, y, w, h, 0, 0, 1, 1, tl, tr, bl, br); }

/* corner quadrant of the disc texture: q 0=tl 1=tr 2=bl 3=br */
static void corner(float x, float y, float r, int q, uint32_t c) {
    float u0 = (q & 1) ? 0.5f : 0, v0 = (q & 2) ? 0.5f : 0;
    quad(x, y, r, r, u0, v0, u0 + 0.5f, v0 + 0.5f, c, c);
}
void draw_rect_round_grad(float x, float y, float w, float h, float r, uint32_t top, uint32_t bottom) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) { draw_rect_grad(x, y, w, h, top, bottom); return; }
    uint32_t ct = col_mix(top, bottom, r / h), cb = col_mix(top, bottom, 1 - r / h);
    bind_flat();
    quad(x + r, y, w - 2 * r, r, 0, 0, 1, 1, top, ct);
    quad(x, y + r, w, h - 2 * r, 0, 0, 1, 1, ct, cb);
    quad(x + r, y + h - r, w - 2 * r, r, 0, 0, 1, 1, cb, bottom);
    bind_tex(&tex_disc);
    quad4(x, y, r, r, 0, 0, 0.5f, 0.5f, top, top, ct, ct);
    quad4(x + w - r, y, r, r, 0.5f, 0, 1, 0.5f, top, top, ct, ct);
    quad4(x, y + h - r, r, r, 0, 0.5f, 0.5f, 1, cb, cb, bottom, bottom);
    quad4(x + w - r, y + h - r, r, r, 0.5f, 0.5f, 1, 1, cb, cb, bottom, bottom);
}
void draw_rect_round(float x, float y, float w, float h, float r, uint32_t argb) { draw_rect_round_grad(x, y, w, h, r, argb, argb); }

void draw_rect_round_outline(float x, float y, float w, float h, float r, uint32_t c) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    bind_flat();
    quad(x + r, y, w - 2 * r, 1.5f, 0, 0, 1, 1, c, c);
    quad(x + r, y + h - 1.5f, w - 2 * r, 1.5f, 0, 0, 1, 1, c, c);
    quad(x, y + r, 1.5f, h - 2 * r, 0, 0, 1, 1, c, c);
    quad(x + w - 1.5f, y + r, 1.5f, h - 2 * r, 0, 0, 1, 1, c, c);
    if (r >= 2) {
        bind_tex(&tex_ring);
        corner(x, y, r, 0, c); corner(x + w - r, y, r, 1, c); corner(x, y + h - r, r, 2, c); corner(x + w - r, y + h - r, r, 3, c);
    }
}

/* 9-slice of the blurred box: the box edge sits 20px inside the texture, so a
 * spread of S px maps to scaling the 32px corner to (12 + S) px. */
void draw_shadow(float x, float y, float w, float h, float spread, uint32_t c) {
    float k = (12 + spread) / 32.0f, cs = 32 * k;
    float ox = x - spread, oy = y - spread, ow = w + 2 * spread, oh = h + 2 * spread;
    if (ow < 2 * cs || oh < 2 * cs) return;
    bind_tex(&tex_shadow);
    quad(ox, oy, cs, cs, 0, 0, 0.5f, 0.5f, c, c);
    quad(ox + ow - cs, oy, cs, cs, 0.5f, 0, 1, 0.5f, c, c);
    quad(ox, oy + oh - cs, cs, cs, 0, 0.5f, 0.5f, 1, c, c);
    quad(ox + ow - cs, oy + oh - cs, cs, cs, 0.5f, 0.5f, 1, 1, c, c);
    quad(ox + cs, oy, ow - 2 * cs, cs, 0.5f, 0, 0.5f, 0.5f, c, c);
    quad(ox + cs, oy + oh - cs, ow - 2 * cs, cs, 0.5f, 0.5f, 0.5f, 1, c, c);
    quad(ox, oy + cs, cs, oh - 2 * cs, 0, 0.5f, 0.5f, 0.5f, c, c);
    quad(ox + ow - cs, oy + cs, cs, oh - 2 * cs, 0.5f, 0.5f, 1, 0.5f, c, c);
    quad(ox + cs, oy + cs, ow - 2 * cs, oh - 2 * cs, 0.5f, 0.5f, 0.5f, 0.5f, c, c);
}
void draw_glow(float cx, float cy, float radius, uint32_t c) { bind_tex(&tex_glow); quad(cx - radius, cy - radius, 2 * radius, 2 * radius, 0, 0, 1, 1, c, c); }
void draw_circle(float cx, float cy, float r, uint32_t c) { bind_tex(&tex_disc); quad(cx - r, cy - r, 2 * r, 2 * r, 0, 0, 1, 1, c, c); }
void draw_ring(float cx, float cy, float r, uint32_t c) { bind_tex(&tex_ring); quad(cx - r, cy - r, 2 * r, 2 * r, 0, 0, 1, 1, c, c); }

static void quad_pts(const float *p, float u0, float v0, float u1, float v1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) {
    float zz = draw_z_next();
    pvr_vertex_t v;
    v.flags = PVR_CMD_VERTEX; v.z = zz; v.oargb = 0;
    v.x = p[4]; v.y = p[5]; v.u = u0; v.v = v1; v.argb = bl; pvr_prim(&v, sizeof v);
    v.x = p[0]; v.y = p[1]; v.u = u0; v.v = v0; v.argb = tl; pvr_prim(&v, sizeof v);
    v.x = p[6]; v.y = p[7]; v.u = u1; v.v = v1; v.argb = br; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = p[2]; v.y = p[3]; v.u = u1; v.v = v0; v.argb = tr; pvr_prim(&v, sizeof v);
}
void draw_tex_fan(const texture_t *t, float cx, float cy, float r, float angle, uint32_t tint) {
    if (!t || !t->ptr) return;
    bind_tex(t);
    const int N = 36;
    float uc = 0.5f * t->iw / t->w, vc = 0.5f * t->ih / t->h, zz = draw_z_next();
    pvr_vertex_t v; v.z = zz; v.oargb = 0; v.argb = tint;
    for (int i = 0; i < N; i++) {
        float a0 = angle + i * (2 * (float)M_PI / N), a1 = angle + (i + 1) * (2 * (float)M_PI / N);
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        float ca = cosf(angle), sa = sinf(angle);
        /* texture turns with the disc: uv = centre + R(-angle) * dir */
        v.flags = PVR_CMD_VERTEX; v.x = cx; v.y = cy; v.u = uc; v.v = vc; pvr_prim(&v, sizeof v);
        v.x = cx + r * c0; v.y = cy + r * s0; v.u = uc + uc * (c0 * ca + s0 * sa); v.v = vc + vc * (s0 * ca - c0 * sa); pvr_prim(&v, sizeof v);
        v.flags = PVR_CMD_VERTEX_EOL;
        v.x = cx + r * c1; v.y = cy + r * s1; v.u = uc + uc * (c1 * ca + s1 * sa); v.v = vc + vc * (s1 * ca - c1 * sa); pvr_prim(&v, sizeof v);
    }
}
void draw_tex_pts(const texture_t *t, const float *xy, float u0, float v0, float u1, float v1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) {
    if (!t || !t->ptr) return;
    bind_tex(t); quad_pts(xy, u0, v0, u1, v1, tl, tr, bl, br);
}
void draw_rect_pts(const float *xy, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br) { bind_flat(); quad_pts(xy, 0, 0, 1, 1, tl, tr, bl, br); }

/* ---------- textured ---------- */
void draw_tex_uv(const texture_t *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t tint) {
    if (!t || !t->ptr) return;
    bind_tex(t); quad(x, y, w, h, u0, v0, u1, v1, tint, tint);
}
void draw_tex(const texture_t *t, float x, float y, float w, float h, uint32_t tint) {
    if (!t || !t->ptr) return;
    draw_tex_uv(t, x, y, w, h, 0, 0, (float)t->iw / t->w, (float)t->ih / t->h, tint);
}
void draw_tex_grad(const texture_t *t, float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    if (!t || !t->ptr) return;
    bind_tex(t); quad(x, y, w, h, 0, 0, (float)t->iw / t->w, (float)t->ih / t->h, top, bottom);
}
void draw_tex_reflect(const texture_t *t, float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    if (!t || !t->ptr) return;
    float vmax = (float)t->ih / t->h;
    bind_tex(t); quad(x, y, w, h, 0, vmax, (float)t->iw / t->w, vmax - vmax * (h / w), top, bottom);
}

/* ---------- text ---------- */
static void text_run(const font_t *f, float x, float y, float scale, float spacing, uint32_t argb, const char *s) {
    if (!f || !f->tex.ptr) return;
    bind_tex(&f->tex);
    float fw = f->tex.w, fh = f->tex.h;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 32) continue;
        const typeof(f->g[0]) *g = &f->g[*p - 32];
        if (g->w && g->h)
            quad(x + g->xoff * scale, y + g->yoff * scale, g->w * scale, g->h * scale,
                 g->x / fw, g->y / fh, (g->x + g->w) / fw, (g->y + g->h) / fh, argb, argb);
        x += g->adv * scale + spacing;
    }
}
void draw_text_scaled(const font_t *f, float x, float y, float scale, uint32_t argb, const char *s) { text_run(f, x, y, scale, 0, argb, s); }
void draw_text(const font_t *f, float x, float y, uint32_t argb, const char *s) { text_run(f, x, y, 1.0f, 0, argb, s); }
void draw_text_spaced(const font_t *f, float x, float y, float spacing, uint32_t argb, const char *s) { text_run(f, x, y, 1.0f, spacing, argb, s); }
void draw_text_shadow(const font_t *f, float x, float y, uint32_t argb, uint32_t shadow, const char *s) { text_run(f, x + 1, y + 2, 1.0f, 0, shadow, s); text_run(f, x, y, 1.0f, 0, argb, s); }

float text_width_spaced(const font_t *f, float spacing, const char *s) {
    float w = 0; for (const unsigned char *p = (const unsigned char *)s; *p; p++) if (*p >= 32) w += f->g[*p - 32].adv + spacing;
    return w;
}
float text_width_scaled(const font_t *f, float scale, const char *s) {
    float w = 0; for (const unsigned char *p = (const unsigned char *)s; *p; p++) if (*p >= 32) w += f->g[*p - 32].adv * scale;
    return w;
}
float text_width(const font_t *f, const char *s) { return text_width_scaled(f, 1.0f, s); }

void draw_text_clipped(const font_t *f, float x, float y, float max_w, uint32_t argb, const char *s) {
    if (text_width(f, s) <= max_w) { draw_text(f, x, y, argb, s); return; }
    char buf[128]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    float ell = text_width(f, "...");
    int n = strlen(buf);
    while (n > 0 && text_width(f, buf) + ell > max_w) buf[--n] = 0;
    strcat(buf, "...");
    draw_text(f, x, y, argb, buf);
}
void draw_text_center(const font_t *f, float cx, float y, uint32_t argb, const char *s) { draw_text(f, cx - text_width(f, s) / 2, y, argb, s); }
void draw_text_right(const font_t *f, float rx, float y, uint32_t argb, const char *s) { draw_text(f, rx - text_width(f, s), y, argb, s); }
void draw_text_right_clipped(const font_t *f, float rx, float y, float max_w, uint32_t argb, const char *s) {
    if (text_width(f, s) <= max_w) { draw_text_right(f, rx, y, argb, s); return; }
    char buf[128]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    float ell = text_width(f, "..."); int n = strlen(buf);
    while (n > 0 && text_width(f, buf) + ell > max_w) buf[--n] = 0;
    strcat(buf, "..."); draw_text_right(f, rx, y, argb, buf);
}

void draw_set_clip(int x, int y, int w, int h) { clip_on = 1; clip_x = x; clip_y = y; clip_w = w; clip_h = h; }
void draw_clear_clip(void) { clip_on = 0; }
