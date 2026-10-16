/* Thin PVR drawing layer: colored quads, textured quads, text, clip, plus the
 * soft primitives the UI is built from (rounded corners, shadows, glows). */
#pragma once
#include <stdint.h>
#include <dc/pvr.h>

typedef struct {
    pvr_ptr_t ptr;
    uint16_t w, h;      /* texture dims (power of two) */
    uint16_t iw, ih;    /* image dims actually used */
    uint32_t fmt;       /* PVR_TXRFMT_* flags */
} texture_t;

typedef struct {
    texture_t tex;
    uint8_t  line_h, base;
    struct { uint16_t x, y; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; } g[224]; /* chars 32..255 */
} font_t;

#define RGBA(r,g,b,a) ((uint32_t)(a) << 24 | (uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))
#define ALPHA(c, a)   (((c) & 0x00FFFFFF) | ((uint32_t)(a) << 24))
#define SCREEN_W 640
#define SCREEN_H 480
/* TV-safe area: CRTs crop ~5% per edge */
#define SAFE_X   32.0f
#define SAFE_TOP 20.0f
#define BOTTOM_Y 424.0f   /* hint bar hairline; text sits at +16 */
#define CONTENT_Y 70.0f
#define CONTENT_H 338.0f  /* CONTENT_Y + CONTENT_H = 408 */

void draw_init(void);
void draw_frame_begin(void);                /* opaque list */
void draw_frame_tr(void);                   /* switch to translucent list */
void draw_frame_end(void);
float draw_z_next(void);

/* colors */
uint32_t col_mix(uint32_t a, uint32_t b, float t);     /* lerp rgb+alpha */
uint32_t col_lighten(uint32_t c, float t);              /* toward white, keeps alpha */
uint32_t col_darken(uint32_t c, float t);               /* toward black, keeps alpha */
uint32_t col_scale_alpha(uint32_t c, float k);
int      col_luma(uint32_t c);                            /* 0..255 */
uint32_t col_on(uint32_t bg);                            /* readable text color for bg */

/* flat */
void draw_rect(float x, float y, float w, float h, uint32_t argb);
void draw_rect_grad(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void draw_rect_grad_h(float x, float y, float w, float h, uint32_t left, uint32_t right);
void draw_rect_grad4(float x, float y, float w, float h, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br);
void draw_rect_round(float x, float y, float w, float h, float r, uint32_t argb);
void draw_rect_round_grad(float x, float y, float w, float h, float r, uint32_t top, uint32_t bottom);
void draw_rect_round_outline(float x, float y, float w, float h, float r, uint32_t argb); /* 1.5px ring */
void draw_shadow(float x, float y, float w, float h, float spread, uint32_t argb);        /* soft drop shadow around rect */
void draw_glow(float cx, float cy, float radius, uint32_t argb);                          /* radial soft blob */
void draw_circle(float cx, float cy, float r, uint32_t argb);
void draw_ring(float cx, float cy, float r, uint32_t argb);

/* textured */
void draw_tex(const texture_t *t, float x, float y, float w, float h, uint32_t tint);
void draw_tex_uv(const texture_t *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t tint);
void draw_tex_grad(const texture_t *t, float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void draw_tex_reflect(const texture_t *t, float x, float y, float w, float h, uint32_t top, uint32_t bottom); /* vertically flipped */

/* arbitrary quads (perspective covers). xy = 8 floats: tl, tr, bl, br. No clipping. */
void draw_tex_pts(const texture_t *t, const float *xy, float u0, float v0, float u1, float v1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br);
void draw_rect_pts(const float *xy, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br);
/* textured disc: the whole image mapped onto a circle of radius r, rotated by angle (rad) */
void draw_tex_fan(const texture_t *t, float cx, float cy, float r, float angle, uint32_t tint);

/* text */
void draw_text(const font_t *f, float x, float y, uint32_t argb, const char *s);
void draw_text_scaled(const font_t *f, float x, float y, float scale, uint32_t argb, const char *s);
void draw_text_spaced(const font_t *f, float x, float y, float spacing, uint32_t argb, const char *s);
float text_width(const font_t *f, const char *s);
float text_width_scaled(const font_t *f, float scale, const char *s);
float text_width_spaced(const font_t *f, float spacing, const char *s);
void draw_text_clipped(const font_t *f, float x, float y, float max_w, uint32_t argb, const char *s);
void draw_text_center(const font_t *f, float cx, float y, uint32_t argb, const char *s);
void draw_text_right(const font_t *f, float rx, float y, uint32_t argb, const char *s);
void draw_text_right_clipped(const font_t *f, float rx, float y, float max_w, uint32_t argb, const char *s);
void draw_text_shadow(const font_t *f, float x, float y, uint32_t argb, uint32_t shadow, const char *s);
void draw_set_clip(int x, int y, int w, int h);
void draw_clear_clip(void);

/* texture.c */
int  texture_load(const char *path, texture_t *out);   /* .tex or .pvr */
void texture_free(texture_t *t);
int  texture_solid(texture_t *out);                      /* 8x8 white */
int  texture_from_alpha(texture_t *out, const uint8_t *alpha, int w, int h); /* white, ARGB4444 */
/* font.c */
int  font_load(const char *base, font_t *out);           /* base.tex + base.fnt */
