/* Shared chrome for every screen: glass panels, labels, button glyphs, hint bar,
 * background, top bar (profile, clock, signal), badges, covers, toasts. */
#pragma once
#include "draw.h"
#include "theme.h"
#include "lang.h"
#include "../list.h"

extern float ui_t;          /* seconds since boot (animations) */
extern float ui_dt;
extern float ui_enter;      /* 0..1 entrance animation of the current screen */

float enter_dy(void);
void panel(float x, float y, float w, float h);
void section_label(float x, float y, const char *s);
void hairline(float x, float y, float w);
uint32_t button_color(const char *b);
float draw_button_glyph(float x, float cy, const char *b, int alpha);
void draw_hints(float cx, float cy, const char *hints, int alpha);
void draw_background(void);
void draw_background_tr(void);
void draw_logo(float x, float y, float size, int alpha);
void draw_wordmark(float x, float y, int alpha);                  /* "Cast Menu" logo + text */
void draw_topbar(const char *title, const char *subtitle);
void draw_topbar_status(void);                                     /* clock + signal, right side */
float chip(float rx, float y, uint32_t bg, uint32_t fg, const char *s);
void draw_bottombar(const char *hint);
void draw_badge(float x, float y, uint32_t bg, const char *txt);
float region_badges(float x, float y, const game_t *g);
void draw_cover(const game_t *g, float x, float y, float size, int reflect);
void draw_avatar(float x, float y, float size);
void fmt_date(const game_t *g, char *out, int cap);
void fmt_playtime(uint16_t minutes, char *out, int cap);
int  draw_text_wrap(const font_t *f, float x, float y, float w, float lh, uint32_t col, const char *s);
void toggle_switch(float x, float y, int on, int alpha);
void pill_button(float x, float y, float w, int sel, const char *label);
void dialog_box(const char *title, float w, float h);
void show_toast(const char *s, int kind);                         /* 0 info, 1 star, 2 ok, 3 error */
int  toast_active(void);
void toast_draw(void);
void draw_textbox(float x, float y, float w, float h, const char *text, const char *placeholder, int focused);
