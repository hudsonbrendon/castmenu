/* Cast Menu UI state machine: boot, browse (coverflow / list / grid with
 * categories), details, last-played sidebar, settings, system info, Cast Chat,
 * Cast AI, dialogs, launch, screensaver. 640x480, PVR-rendered. */
#include "ui.h"
#include "draw.h"
#include "theme.h"
#include "input.h"
#include "covers.h"
#include "anim.h"
#include "lang.h"
#include "chrome.h"
#include "coverflow.h"
#include "screens.h"
#include "osk.h"
#include "../list.h"
#include "../settings.h"
#include "../launch.h"
#include "../version.h"
#include "../voice.h"
#include "../net/netif.h"
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>

typedef enum { ST_BOOT, ST_BROWSE, ST_DETAILS, ST_SIDEBAR, ST_SETTINGS, ST_SYSINFO, ST_CHAT, ST_AI, ST_DIALOG_EXIT, ST_DIALOG_DISC, ST_LETTER, ST_LAUNCH, ST_SAVER } state_t;

static state_t state = ST_BOOT, prev_state;
static input_t in;
static sysinfo_t sys;
static uint64_t last_t;
static int boot_pct; static char boot_msg[64];
static int cursor, sel_settings, sel_dialog, letter_idx;
static float scroll_y, scroll_target, sel_anim, fade, launch_fade, set_scroll, set_scroll_target, cf_pos, cat_anim; static int cat_dir;
static game_t *disc_set[8]; static int disc_set_n;
static const game_t *launch_target;
static int exit_flag;
static char avatars[16][AVATAR_LEN]; static int n_avatars;

#define LIST_X SAFE_X
#define LIST_Y CONTENT_Y
#define LIST_W 336.0f
#define LIST_H CONTENT_H
#define ROW_H 26.0f
#define LIST_ROWS 12
#define PANEL_X (SAFE_X + LIST_W + 16)
#define PANEL_W (SCREEN_W - SAFE_X - PANEL_X)
#define GRID_COLS 5
#define GRID_ROWS 3
#define GRID_CELL 104.0f
#define GRID_PAD 10.0f

static int view_mode(void) { return settings_get()->view; }   /* 0 coverflow, 1 list, 2 grid */
static void go(state_t s) { prev_state = state; state = s; ui_enter = 0; }
static const char *cat_label(int cat) {
    return cat == CAT_ALL ? tr(S_CAT_ALL) : cat == CAT_FAVORITES ? tr(S_CAT_FAVORITES) : cat == CAT_RECENT ? tr(S_CAT_RECENT) : list_category_name(cat);
}

/* ---------- boot ---------- */
void ui_boot_progress(int pct, const char *msg) {
    boot_pct = pct; strncpy(boot_msg, msg, sizeof boot_msg - 1);
    draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr();
    theme_t *t = &g_theme;
    float pulse = 0.5f + 0.5f * sinf(ui_t * 3);
    draw_glow(SCREEN_W / 2, 170, 150 + 30 * pulse, col_scale_alpha(t->glow, 0.8f));
    draw_ring(SCREEN_W / 2, 170, 70 + 10 * pulse, ALPHA(t->accent, 60)); draw_ring(SCREEN_W / 2, 170, 52, ALPHA(t->accent, 90));
    /* controller lockup: swirl on top, name below in logotype grey */
    draw_logo(SCREEN_W / 2 - 44, 118, 88, 255);
    draw_text_shadow(&g_font_display, SCREEN_W / 2 - text_width(&g_font_display, "Cast Menu") / 2, 228, t->text_dim, ALPHA(t->text_hi, 60), "Cast Menu");
    draw_text_spaced(&g_font_small, SCREEN_W / 2 - text_width_spaced(&g_font_small, 2, tr(S_READING_LIST)) / 2, 284, 2, t->text_dim, tr(S_READING_LIST));
    float bx = 170, by = 312, bw = 300, f = clampf(pct / 100.0f, 0, 1);
    draw_rect_round(bx, by, bw, 8, 4, ALPHA(t->panel_alt, 255));
    draw_rect_round_outline(bx, by, bw, 8, 4, ALPHA(t->text_hi, 20));
    if (f > 0) { draw_rect_round_grad(bx, by, bw * f, 8, 4, col_lighten(t->accent, 0.25f), t->accent); draw_glow(bx + bw * f, by + 4, 16, t->glow); }
    draw_text_center(&g_font_small, SCREEN_W / 2, 330, t->text_dim, boot_msg);
    draw_text_center(&g_font_small, SCREEN_W / 2, BOTTOM_Y + 10, ALPHA(t->text_dim, 120), "v" CAST_VERSION);
    draw_frame_end();
}

/* ---------- browse ---------- */
static void browse_clamp(void) {
    if (cursor >= g_list.view_count) cursor = g_list.view_count - 1;
    if (cursor < 0) cursor = 0;
}
static void draw_detail_panel(const game_t *g) {
    theme_t *t = &g_theme;
    float x = PANEL_X, y = LIST_Y + enter_dy();
    panel(x, y, PANEL_W, LIST_H);
    if (!g) { draw_text_center(&g_font, x + PANEL_W / 2, y + 160, t->text_dim, tr(S_NO_GAMES)); return; }
    float cs = 120;
    draw_cover(g, x + (PANEL_W - cs) / 2, y + 14, cs, 1);
    float ty = y + 14 + cs + 34;
    draw_set_clip((int)x + 8, (int)ty - 2, (int)PANEL_W - 16, 24);
    draw_text_clipped(&g_font, x + 14, ty, PANEL_W - 28, t->text_hi, g->name);
    draw_clear_clip();
    ty += 24; region_badges(x + 14, ty, g); ty += 24;
    hairline(x + 14, ty, PANEL_W - 28); ty += 8;
    char buf[48], d[16], pt[24]; fmt_date(g, d, sizeof d); snprintf(buf, sizeof buf, "%02d", g->slot);
    fmt_playtime(settings_get()->playtime[g->slot < 1000 ? g->slot : 0], pt, sizeof pt);
    const struct { const char *k; const char *v; } rows[] = {
        { tr(S_DATE), d }, { tr(S_PUBLISHER), g->publisher[0] ? g->publisher : NULL }, { tr(S_PRODUCT), g->product[0] ? g->product : "-" },
        { tr(S_GENRE), g->genre[0] ? g->genre : NULL }, { tr(S_PLAY_TIME), pt }, { tr(S_FOLDER), buf },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (!rows[i].v) continue;
        if (ty + 16 > y + LIST_H - 48) break;              /* keep clear of the button */
        draw_text(&g_font_small, x + 14, ty, t->text_dim, rows[i].k);
        float kw = text_width(&g_font_small, rows[i].k);
        draw_text_right_clipped(&g_font_small, x + PANEL_W - 14, ty, PANEL_W - 28 - kw - 8, t->text, rows[i].v);
        ty += 16;
    }
    float by = y + LIST_H - 40, bw = PANEL_W - 28;
    draw_shadow(x + 14, by, bw, 26, 6, col_scale_alpha(t->glow, 0.8f));
    draw_rect_round_grad(x + 14, by, bw, 26, 13, col_lighten(t->accent, 0.2f), t->accent);
    draw_text_center(&g_font_small, x + PANEL_W / 2 + 6, by + 6, t->sel_text, tr(S_PLAY));
    draw_button_glyph(x + 14 + bw / 2 - text_width(&g_font_small, tr(S_PLAY)) / 2 - 20, by + 13, "A", 255);
    if (g->favorite) draw_text(&g_font_small, x + PANEL_W - 30, y + 20, 0xFFFFD166, "\x2a");
}
static void draw_list(void) {
    theme_t *t = &g_theme;
    float ly = LIST_Y + enter_dy();
    panel(LIST_X, ly, LIST_W, LIST_H);
    if (!g_list.view_count) { draw_text_center(&g_font, LIST_X + LIST_W / 2, ly + 160, t->text_dim, tr(S_NO_GAMES)); return; }
    draw_set_clip((int)LIST_X + 2, (int)ly + 6, (int)LIST_W - 4, (int)LIST_H - 12);
    float base = ly + 14 - scroll_y;
    int first = (int)(scroll_y / ROW_H) - 1; if (first < 0) first = 0;
    int last = first + LIST_ROWS + 2; if (last > g_list.view_count) last = g_list.view_count;
    for (int i = first; i < last; i++) {
        game_t *g = list_view_item(i);
        float y = base + i * ROW_H, sx = LIST_X + 8, sw = LIST_W - 22;
        if (i & 1) draw_rect_round(sx, y, sw, ROW_H, 4, ALPHA(t->text_hi, 5));
        if (i == cursor) {
            float pulse = 0.5f + 0.5f * sinf(ui_t * 3);
            draw_glow(sx + sw / 2, y + ROW_H / 2, sw * 0.6f, col_scale_alpha(t->glow, 0.35f + 0.15f * pulse));
            draw_rect_round_grad(sx, y + 1, sw, ROW_H - 2, 6, col_lighten(t->sel_bg, 0.18f), t->sel_bg);
            draw_rect_round_outline(sx, y + 1, sw, ROW_H - 2, 6, ALPHA(0xFFFFFF, 60));
            draw_rect_round(sx + 3, y + 7, 3, ROW_H - 14, 1.5f, t->text_hi);
        }
        uint32_t col = i == cursor ? t->sel_text : t->text;
        char num[8]; snprintf(num, sizeof num, "%02d", g->slot);
        draw_text(&g_font_small, LIST_X + 20, y + 8, i == cursor ? ALPHA(t->sel_text, 150) : ALPHA(t->text_dim, 160), num);
        draw_text_clipped(&g_font, LIST_X + 50, y + 5, LIST_W - 130, col, g->name);
        float bx = LIST_X + LIST_W - 22;
        if (g->favorite) { bx -= 14; draw_text(&g_font_small, bx, y + 7, i == cursor ? t->sel_text : 0xFFFFD166, "\x2a"); }
        for (int r = strlen(g->region) - 1; r >= 0; r--) { bx -= 19; char s[2] = { g->region[r], 0 }; draw_badge(bx, y + 6, g->region[r] == 'J' ? t->badge_j : g->region[r] == 'U' ? t->badge_u : t->badge_e, s); }
    }
    draw_clear_clip();
    draw_rect_grad(LIST_X + 2, ly + 2, LIST_W - 4, 12, ALPHA(t->bg, 160), ALPHA(t->bg, 0));
    draw_rect_grad(LIST_X + 2, ly + LIST_H - 14, LIST_W - 4, 12, ALPHA(t->bg, 0), ALPHA(t->bg, 160));
    if (g_list.view_count > LIST_ROWS) {
        float track = LIST_H - 24, th = track * LIST_ROWS / g_list.view_count; if (th < 20) th = 20;
        float ty = ly + 12 + (track - th) * scroll_y / ((g_list.view_count - LIST_ROWS) * ROW_H);
        draw_rect_round(LIST_X + LIST_W - 9, ly + 12, 3, track, 1.5f, ALPHA(t->text_dim, 40));
        draw_rect_round(LIST_X + LIST_W - 9, ty, 3, th, 1.5f, t->accent);
    }
}
static void draw_grid(void) {
    theme_t *t = &g_theme;
    if (!g_list.view_count) { draw_text_center(&g_font, SCREEN_W / 2, 220, t->text_dim, tr(S_NO_GAMES)); return; }
    float gx = (SCREEN_W - GRID_COLS * GRID_CELL - (GRID_COLS - 1) * GRID_PAD) / 2, gy = CONTENT_Y - 2 + enter_dy();
    int row0 = cursor / GRID_COLS - 1; if (row0 < 0) row0 = 0;
    int max_row = (g_list.view_count + GRID_COLS - 1) / GRID_COLS - GRID_ROWS; if (max_row < 0) max_row = 0;
    if (row0 > max_row) row0 = max_row;
    scroll_target = row0 * (GRID_CELL + GRID_PAD);
    int cx0 = (int)gx - 12, cy0 = (int)gy - 8, cw = (int)(GRID_COLS * (GRID_CELL + GRID_PAD)) + 12, ch = (int)(GRID_ROWS * (GRID_CELL + GRID_PAD)) + 4;
    draw_set_clip(cx0, cy0, cw, ch);
    for (int pass = 0; pass < 2; pass++)
    for (int i = (row0 - 1) * GRID_COLS; i < (row0 + GRID_ROWS + 1) * GRID_COLS; i++) {
        if (i < 0 || i >= g_list.view_count) continue;
        int sel = i == cursor;
        if ((pass == 0) == (sel != 0)) continue;
        game_t *g = list_view_item(i);
        float x = gx + (i % GRID_COLS) * (GRID_CELL + GRID_PAD), y = gy + (i / GRID_COLS) * (GRID_CELL + GRID_PAD) - scroll_y;
        float grow = sel ? 6 * ease_out(sel_anim) : 0;
        float cx = x - grow, cy = y - grow, cs = GRID_CELL + 2 * grow;
        if (sel) { draw_glow(cx + cs / 2, cy + cs / 2, cs * 0.85f, t->glow); draw_shadow(cx, cy, cs, cs, 14, t->shadow); }
        draw_rect_round_grad(cx, cy, cs, cs, 8, col_lighten(t->panel, 0.08f), col_darken(t->panel, 0.15f));
        float cov = cs - 24;
        const texture_t *c = settings_get()->cover_mode ? cover_get(g->slot) : NULL;
        draw_set_clip((int)cx, (int)cy, (int)cs, (int)cs);
        if (c) draw_tex(c, cx, cy, cs, cs, 0xFFFFFFFF);
        else {
            draw_rect_grad4(cx, cy, cs, cs, col_lighten(t->accent_dim, 0.1f), t->accent_dim, col_darken(t->accent_dim, 0.3f), col_darken(t->panel, 0.2f));
            char ini[2] = { g->name[0] ? g->name[0] : '?', 0 };
            draw_text_scaled(&g_font_display, cx + cs / 2 - text_width_scaled(&g_font_display, 1.2f, ini) / 2, cy + cov / 2 - g_font_display.line_h * 0.6f, 1.2f, ALPHA(t->text_hi, 50), ini);
        }
        draw_rect_grad(cx, cy + cov - 30, cs, 30 + 24, ALPHA(0x000000, 0), ALPHA(0x000000, 210));
        draw_rect_grad(cx, cy, cs, cs * 0.4f, ALPHA(0xFFFFFF, 20), ALPHA(0xFFFFFF, 0));
        draw_clear_clip(); draw_set_clip(cx0, cy0, cw, ch);
        draw_rect_round_outline(cx, cy, cs, cs, 8, sel ? t->accent : ALPHA(t->text_hi, 18));
        draw_set_clip((int)cx, (int)cy, (int)cs, (int)cs);
        draw_text_clipped(&g_font_small, cx + 6, cy + cs - 20, cs - 12, sel ? 0xFFFFFFFF : ALPHA(0xFFFFFF, 220), g->name); /* caption sits on a dark gradient in every theme */
        draw_clear_clip(); draw_set_clip(cx0, cy0, cw, ch);
        float bx = cx + 6;
        for (const char *r = g->region; *r; r++) { char s[2] = { *r, 0 }; draw_badge(bx, cy + 5, *r == 'J' ? t->badge_j : *r == 'U' ? t->badge_u : t->badge_e, s); bx += 19; }
        if (g->favorite) draw_text(&g_font_small, cx + cs - 16, cy + 4, 0xFFFFD166, "\x2a");
        if (sel) draw_rect_round(cx + 6, cy + cs - 8, cs - 12, 2, 1, ALPHA(t->accent, 200));
    }
    draw_clear_clip();
}
static void change_category(int dir) {
    list_set_category(g_list.category + dir); cursor = 0; scroll_target = 0; cf_pos = 0; cat_anim = 0; cat_dir = dir; sel_anim = 0;
}
static void browse_update(void) {
    int n = g_list.view_count;
    if (in.pressed & BTN_X) { sel_settings = 0; go(ST_SETTINGS); return; }
    if (in.pressed & BTN_START) { go(ST_SYSINFO); return; }
    if (in.pressed & BTN_B) { sidebar_open(); go(ST_SIDEBAR); return; }
    if ((in.held & BTN_L) && (in.held & BTN_R) && (in.pressed & (BTN_L | BTN_R))) { letter_idx = 0; go(ST_LETTER); return; }
    if (in.pressed & BTN_L) { change_category(-1); return; }
    if (in.pressed & BTN_R) { change_category(1); return; }
    if (!n) return;
    int grid = view_mode() == 2, cf = view_mode() == 0;
    int step = grid ? GRID_COLS : 1, page = grid ? GRID_COLS * GRID_ROWS : LIST_ROWS;
    int old = cursor;
    if (cf) {
        if (in.repeat & BTN_LEFT) cursor--;
        if (in.repeat & BTN_RIGHT) cursor++;
        if (in.repeat & BTN_UP) cursor -= 5;
        if (in.repeat & BTN_DOWN) cursor += 5;
        if (cursor < 0) cursor = (in.repeat & BTN_LEFT) && old == 0 ? n - 1 : 0;
        if (cursor >= n) cursor = (in.repeat & BTN_RIGHT) && old == n - 1 ? 0 : n - 1;
        if (cursor != old && abs(cursor - old) > 3) cf_pos = cursor - (cursor > old ? 2 : -2);
    } else {
        if (in.repeat & BTN_UP) cursor -= step;
        if (in.repeat & BTN_DOWN) cursor += step;
        if (grid) { if (in.repeat & BTN_LEFT) cursor--; if (in.repeat & BTN_RIGHT) cursor++; }
        else { if (in.repeat & BTN_LEFT) cursor -= page; if (in.repeat & BTN_RIGHT) cursor += page; }
        if (cursor < 0) cursor = (in.repeat & BTN_UP) && old == 0 ? n - 1 : 0;
        if (cursor >= n) cursor = (in.repeat & BTN_DOWN) && old == n - 1 ? 0 : n - 1;
    }
    if (cursor != old) sel_anim = 0;
    game_t *g = list_view_item(cursor);
    if (in.pressed & BTN_A) {
        disc_set_n = list_disc_set(g, disc_set, 8);
        if (disc_set_n > 1) { sel_dialog = 0; go(ST_DIALOG_DISC); }
        else { launch_target = g; launch_fade = 0; go(ST_LAUNCH); }
    }
    if (in.pressed & BTN_Y) { details_open(g); go(ST_DETAILS); }
    if (view_mode() == 1) {
        int top = (int)(scroll_target / ROW_H);
        if (cursor < top) scroll_target = cursor * ROW_H;
        if (cursor >= top + LIST_ROWS) scroll_target = (cursor - LIST_ROWS + 1) * ROW_H;
        float max = (n - LIST_ROWS) * ROW_H; if (max < 0) max = 0;
        scroll_target = clampf(scroll_target, 0, max);
    }
}
static void draw_profile_bar(void) {
    theme_t *t = &g_theme;
    settings_t *s = settings_get();
    draw_rect_grad(0, 0, SCREEN_W, 64, ALPHA(t->bg, 200), ALPHA(t->bg, 0));
    if (s->nick[0] && strcmp(s->nick, "Player")) {
        draw_avatar(SAFE_X, SAFE_TOP - 6, 38);
        draw_text_shadow(&g_font_big, SAFE_X + 48, SAFE_TOP, t->text_hi, ALPHA(0x000000, 90), s->nick);
        if (chat_unread()) { char u[8]; snprintf(u, sizeof u, "%d", chat_unread()); float ux = SAFE_X + 48 + text_width(&g_font_big, s->nick) + 14; draw_circle(ux, SAFE_TOP + 12, 9, t->badge_j); draw_text_center(&g_font_small, ux, SAFE_TOP + 5, 0xFFFFFFFF, u); }
    } else draw_wordmark(SAFE_X, SAFE_TOP - 6, 255);
    draw_topbar_status();
}
static void draw_category_bar(void) {
    theme_t *t = &g_theme;
    /* slingshot: label slides in from the side it came from */
    float a = ease_out(cat_anim), off = (1 - a) * 40 * cat_dir;
    const char *name = cat_label(g_list.category);
    float cy = BOTTOM_Y - 40;
    draw_set_clip((int)SAFE_X, (int)cy - 2, 200, 24);
    draw_text_shadow(&g_font, SAFE_X + off, cy, ALPHA(t->text, (int)(255 * (0.3f + 0.7f * a))), ALPHA(0x000000, 100), name);
    draw_clear_clip();
    float nw = text_width(&g_font, name); if (nw > 196) nw = 196;
    draw_rect_round(SAFE_X + off, cy + 20, nw * a, 2, 1, t->accent);
    int total = list_category_total();
    for (int i = 0; i < total && i < 14; i++) draw_circle(SAFE_X + i * 9 + 3, cy + 29, i == g_list.category ? 3 : 2, i == g_list.category ? t->accent : ALPHA(t->text_dim, 110));
    char pos[24]; snprintf(pos, sizeof pos, "%d/%d", g_list.view_count ? cursor + 1 : 0, g_list.view_count);
    draw_text_shadow(&g_font, SCREEN_W - SAFE_X - text_width(&g_font, pos), cy, t->text, ALPHA(0x000000, 100), pos);
}
static void browse_draw(void) {
    draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr();
    if (view_mode() == 0) { draw_profile_bar(); coverflow_draw(cursor, &cf_pos); draw_category_bar(); }
    else {
        char sub[64]; snprintf(sub, sizeof sub, "%d %s  \xb7  %s", g_list.view_count, tr(S_GAMES_LOWER), cat_label(g_list.category));
        draw_topbar(tr(S_LIBRARY), sub);
        draw_topbar_status();
        if (view_mode() == 2) draw_grid(); else { draw_list(); draw_detail_panel(list_view_item(cursor)); }
    }
    draw_bottombar(tr(S_HINT_BROWSE));
}

/* ---------- settings ---------- */
typedef enum { O_NICK, O_AVATAR, O_REGION, O_VGA, O_IGR, O_INTRO, O_LICENSE, O_THEME, O_VIEW, O_LANG, O_SORT, O_REGIONF, O_MULTI, O_COVERS, O_CHAT, O_AI, O_SERVER, O_NETAUTO, O_CONNECT, O_SAVER, O_SCAN, O_SAVE, O_RESET, O_SYSINFO, O_EXIT, O_COUNT } opt_t;
static char nick_edit[NICK_LEN], server_edit[SERVER_LEN]; static int osk_opt;
static int opt_is_toggle(opt_t o, int *val) {
    settings_t *s = settings_get();
    switch (o) {
    case O_REGION: *val = s->region_free; return 1; case O_VGA: *val = s->force_vga; return 1; case O_IGR: *val = s->igr; return 1;
    case O_INTRO: *val = s->boot_intro; return 1; case O_LICENSE: *val = s->sega_license; return 1;
    case O_MULTI: *val = s->hide_multidisc; return 1; case O_COVERS: *val = s->cover_mode; return 1; case O_NETAUTO: *val = s->net_auto; return 1;
    default: return 0;
    }
}
static int opt_is_action(opt_t o) { return o == O_CHAT || o == O_AI || o == O_CONNECT || o >= O_SAVE; }
static void opt_value(opt_t o, char *out, int cap) {
    settings_t *s = settings_get();
    switch (o) {
    case O_NICK: snprintf(out, cap, "%s", s->nick); break;
    case O_AVATAR: snprintf(out, cap, "%s", s->avatar[0] ? s->avatar : "-"); break;
    case O_THEME: snprintf(out, cap, "%s", g_theme.name); break;
    case O_VIEW: snprintf(out, cap, "%s", s->view == 0 ? tr(S_COVERFLOW) : s->view == 1 ? tr(S_LIST) : tr(S_GRID)); break;
    case O_LANG: snprintf(out, cap, "%s", lang_names[lang_current()]); break;
    case O_SORT: snprintf(out, cap, "%s", list_sort_name(g_list.sort)); break;
    case O_REGIONF: snprintf(out, cap, "%s", g_list.filter_region == 'J' ? tr(S_JAPAN) : g_list.filter_region == 'U' ? tr(S_USA) : g_list.filter_region == 'E' ? tr(S_EUROPE) : tr(S_ALL)); break;
    case O_SERVER: snprintf(out, cap, "%s", s->server); break;
    case O_SAVER: if (s->screensaver_min) snprintf(out, cap, "%d %s", s->screensaver_min, tr(S_MINUTES)); else snprintf(out, cap, "%s", tr(S_OFF)); break;
    case O_SCAN: snprintf(out, cap, "%d ms", s->scan_delay_ms); break;
    case O_CONNECT: { net_status_t ns = net_status(); snprintf(out, cap, "%s", chatnet_online() ? tr(S_CONNECTED) : ns.state == NET_CONNECTING ? tr(S_CONNECTING) : ns.state == NET_NO_ADAPTER ? tr(S_NO_ADAPTER) : tr(S_OFFLINE)); } break;
    default: out[0] = 0;
    }
}
static const char *opt_label(opt_t o) {
    static const str_id ids[O_COUNT] = { S_NICKNAME, S_AVATAR, S_REGION_FREE, S_FORCE_VGA, S_IGR, S_BOOT_INTRO, S_SEGA_LICENSE, S_THEME, S_DEFAULT_VIEW, S_LANGUAGE, S_SORT, S_REGION, S_HIDE_MULTIDISC, S_COVERS, S_CAST_CHAT, S_CAST_AI, S_SERVER, S_NETWORK, S_CONNECT, S_SCREENSAVER, S_SCAN_DELAY, S_SAVE, S_RESET_SETTINGS, S_SYSINFO, S_EXIT_TO_BIOS };
    return tr(ids[o]);
}
static const char *opt_desc(opt_t o) {
    static const str_id ids[O_COUNT] = { S_D_NICK, S_D_AVATAR, S_D_REGION, S_D_VGA, S_D_IGR, S_D_INTRO, S_D_LICENSE, S_D_THEME, S_D_VIEW, S_D_LANG, S_D_SORT, S_D_REGIONF, S_D_MULTI, S_D_COVERS, S_D_CHAT, S_D_AI, S_D_NET, S_D_NET, S_D_NET, S_D_SAVER, S_D_SCAN, S_D_SAVE, S_D_RESET, S_D_SYSINFO, S_D_EXIT };
    return tr(ids[o]);
}
static str_id opt_section(opt_t o) {
    switch (o) { case O_NICK: return S_SEC_PROFILE; case O_REGION: return S_SEC_LOADER; case O_THEME: return S_SEC_VISUAL; case O_SORT: return S_SEC_LIBRARY; case O_CHAT: return S_SEC_ONLINE; case O_SAVER: return S_SEC_SYSTEM; default: return S_COUNT; }
}
static void opt_change(opt_t o, int dir) {
    settings_t *s = settings_get();
    switch (o) {
    case O_NICK: strncpy(nick_edit, s->nick, NICK_LEN - 1); osk_opt = O_NICK; osk_open(tr(S_NICKNAME), nick_edit, NICK_LEN); break;
    case O_AVATAR: {
        int cur = -1; for (int i = 0; i < n_avatars; i++) if (!strcmp(avatars[i], s->avatar)) cur = i;
        cur += dir; if (cur >= n_avatars) cur = -1; if (cur < -1) cur = n_avatars - 1;
        if (cur < 0) s->avatar[0] = 0; else strncpy(s->avatar, avatars[cur], AVATAR_LEN - 1);
    } break;
    case O_REGION: s->region_free ^= 1; break;
    case O_VGA: s->force_vga ^= 1; break;
    case O_IGR: s->igr ^= 1; break;
    case O_INTRO: s->boot_intro ^= 1; break;
    case O_LICENSE: s->sega_license ^= 1; break;
    case O_THEME: { int n = theme_count(); s->theme = (s->theme + dir + n) % n; theme_load(s->theme); } break;
    case O_VIEW: s->view = (s->view + dir + 3) % 3; break;
    case O_LANG: s->lang = (s->lang + dir + LANG_COUNT) % LANG_COUNT; break;
    case O_SORT: g_list.sort = (g_list.sort + dir + SORT_COUNT) % SORT_COUNT; s->sort = g_list.sort; list_rebuild_view(); cursor = 0; scroll_target = 0; break;
    case O_REGIONF: { static const char cyc[] = { 0, 'J', 'U', 'E' }; int i = 0; while (cyc[i] != g_list.filter_region) i++; g_list.filter_region = cyc[(i + dir + 4) % 4]; list_rebuild_view(); cursor = 0; scroll_target = 0; } break;
    case O_MULTI: s->hide_multidisc ^= 1; g_list.hide_multidisc_extra = s->hide_multidisc; list_rebuild_view(); cursor = 0; break;
    case O_COVERS: s->cover_mode ^= 1; break;
    case O_CHAT: chat_open(); go(ST_CHAT); break;
    case O_AI: ai_open(); go(ST_AI); break;
    case O_SERVER: strncpy(server_edit, s->server, SERVER_LEN - 1); osk_opt = O_SERVER; osk_open(tr(S_SERVER), server_edit, SERVER_LEN); break;
    case O_NETAUTO: s->net_auto ^= 1; break;
    case O_CONNECT: chatnet_ensure(); break;
    case O_SAVER: { int v = s->screensaver_min + dir * 5; if (v < 0) v = 30; if (v > 30) v = 0; s->screensaver_min = v; } break;
    case O_SCAN: { int v = s->scan_delay_ms + dir * 250; if (v < 250) v = 250; if (v > 5000) v = 5000; s->scan_delay_ms = v; } break;
    case O_SAVE: { int ok = settings_save() == 0; show_toast(ok ? tr(S_SAVED) : tr(S_SAVE_FAIL), ok ? 2 : 3); } break;
    case O_RESET: { settings_t keep = *s; settings_init(); settings_t def = *s; *s = keep; s->region_free = s->force_vga = s->igr = s->boot_intro = s->sega_license = 1; s->theme = 0; s->view = 0; s->screensaver_min = def.screensaver_min; s->scan_delay_ms = def.scan_delay_ms; theme_load(0); } break;
    case O_SYSINFO: go(ST_SYSINFO); break;
    case O_EXIT: sel_dialog = 1; go(ST_DIALOG_EXIT); break;
    default: break;
    }
}
#define SET_X SAFE_X
#define SET_W 360.0f
#define SET_ROW 24.0f
#define SET_HDR 26.0f
static float opt_row_y(opt_t o) {
    float y = 0;
    for (int i = 0; i <= (int)o; i++) { if (opt_section(i) != S_COUNT) y += SET_HDR; if (i < (int)o) y += SET_ROW; }
    return y;
}
static void settings_update(void) {
    if (osk_active()) {
        int r = osk_update(&in);
        if (r == 1) { settings_t *s = settings_get(); if (osk_opt == O_NICK && nick_edit[0]) strncpy(s->nick, nick_edit, NICK_LEN - 1); if (osk_opt == O_SERVER && server_edit[0]) { strncpy(s->server, server_edit, SERVER_LEN - 1); s->server_user = 1; } }
        return;
    }
    if (in.repeat & BTN_UP) sel_settings = (sel_settings + O_COUNT - 1) % O_COUNT;
    if (in.repeat & BTN_DOWN) sel_settings = (sel_settings + 1) % O_COUNT;
    int text_or_action = sel_settings == O_NICK || sel_settings == O_SERVER || opt_is_action(sel_settings);
    if (in.pressed & BTN_A) opt_change(sel_settings, 1);
    if ((in.pressed & BTN_RIGHT) && !text_or_action) opt_change(sel_settings, 1);
    if ((in.pressed & BTN_LEFT) && !text_or_action) opt_change(sel_settings, -1);
    if (in.pressed & BTN_START) opt_change(O_SAVE, 1);
    if (in.pressed & BTN_B) { go(ST_BROWSE); list_rebuild_view(); browse_clamp(); }
    float ry = opt_row_y(sel_settings), vis = LIST_H - 24;
    if (ry - set_scroll_target < 0) set_scroll_target = ry - (opt_section(sel_settings) != S_COUNT ? SET_HDR : 0);
    if (ry + SET_ROW - set_scroll_target > vis) set_scroll_target = ry + SET_ROW - vis;
    if (set_scroll_target < 0) set_scroll_target = 0;
}
static void settings_draw(void) {
    theme_t *t = &g_theme;
    draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr();
    draw_topbar(tr(S_SETTINGS), NULL); draw_topbar_status();
    float x = SET_X, y = LIST_Y + enter_dy(), w = SET_W;
    panel(x, y, w, LIST_H);
    draw_set_clip((int)x + 2, (int)y + 8, (int)w - 4, (int)LIST_H - 16);
    float cy = y + 12 - set_scroll;
    for (int i = 0; i < O_COUNT; i++) {
        if (opt_section(i) != S_COUNT) { section_label(x + 16, cy + 6, tr(opt_section(i))); cy += SET_HDR; }
        int sel = i == sel_settings;
        float rx = x + 10, rw = w - 22;
        if (sel) {
            draw_glow(rx + rw / 2, cy + SET_ROW / 2, rw * 0.5f, col_scale_alpha(t->glow, 0.4f));
            draw_rect_round_grad(rx, cy, rw, SET_ROW, 6, col_lighten(t->sel_bg, 0.18f), t->sel_bg);
            draw_rect_round_outline(rx, cy, rw, SET_ROW, 6, ALPHA(0xFFFFFF, 60));
        }
        uint32_t fg = sel ? t->sel_text : t->text;
        draw_text(&g_font, rx + 12, cy + 3, i == O_EXIT && !sel ? t->badge_j : fg, opt_label(i));
        int tv;
        if (opt_is_toggle(i, &tv)) toggle_switch(rx + rw - 46, cy + 3, tv, 255);
        else {
            char v[40]; opt_value(i, v, sizeof v);
            if (v[0]) {
                float vw = text_width(&g_font_small, v) + 16; if (vw > 190) vw = 190;
                draw_rect_round(rx + rw - 12 - vw, cy + 4, vw, 17, 8, sel ? ALPHA(0x000000, 60) : ALPHA(t->panel_alt, 255));
                if (!sel) draw_rect_round_outline(rx + rw - 12 - vw, cy + 4, vw, 17, 8, ALPHA(t->accent, 90));
                draw_text_clipped(&g_font_small, rx + rw - 12 - vw + 8, cy + 6, vw - 16, sel ? t->sel_text : t->accent, v);
                if (sel && !opt_is_action(i)) { draw_text(&g_font_small, rx + rw - 12 - vw - 12, cy + 6, t->sel_text, "<"); draw_text(&g_font_small, rx + rw - 8, cy + 6, t->sel_text, ">"); }
            } else draw_text(&g_font, rx + rw - 22, cy + 3, sel ? t->sel_text : t->text_dim, "\xbb");
        }
        cy += SET_ROW;
    }
    draw_clear_clip();
    draw_rect_grad(x + 2, y + 2, w - 4, 10, ALPHA(t->bg, 150), ALPHA(t->bg, 0));
    draw_rect_grad(x + 2, y + LIST_H - 12, w - 4, 10, ALPHA(t->bg, 0), ALPHA(t->bg, 150));
    /* info card */
    float ix = SET_X + SET_W + 16, iw = SCREEN_W - SAFE_X - ix, iy = y;
    panel(ix, iy, iw, LIST_H);
    section_label(ix + 14, iy + 14, tr(S_HELP_SETTINGS));
    draw_set_clip((int)ix + 4, (int)iy + 4, (int)iw - 8, (int)LIST_H - 8);
    int nl = draw_text_wrap(&g_font, ix + 14, iy + 36, iw - 28, 20, t->text_hi, opt_label(sel_settings));
    float dy = iy + 36 + nl * 20 + 6;
    hairline(ix + 14, dy, iw - 28); dy += 10;
    nl = draw_text_wrap(&g_font_small, ix + 14, dy, iw - 28, 15, t->text_dim, opt_desc(sel_settings));
    dy += nl * 15 + 14;
    if (sel_settings == O_THEME) {
        const uint32_t sw[] = { t->bg, t->panel, t->accent, t->accent2, t->text };
        for (int i = 0; i < 5; i++) { draw_circle(ix + 24 + i * 24, dy + 10, 9, sw[i]); draw_ring(ix + 24 + i * 24, dy + 10, 9, ALPHA(t->text_hi, 60)); }
        dy += 30; draw_text(&g_font_small, ix + 14, dy, t->text_dim, t->author); dy += 16;
        char cnt[32]; snprintf(cnt, sizeof cnt, "%d / %d", settings_get()->theme + 1, theme_count()); draw_text(&g_font_small, ix + 14, dy, t->text_dim, cnt);
    }
    if (sel_settings == O_NICK || sel_settings == O_AVATAR) { draw_avatar(ix + iw / 2 - 32, dy + 4, 64); draw_text_center(&g_font, ix + iw / 2, dy + 76, t->text_hi, settings_get()->nick); }
    if (sel_settings == O_LANG) for (int i = 0; i < LANG_COUNT; i++) { int cur = i == lang_current(); draw_rect_round(ix + 14, dy + i * 20, iw - 28, 18, 6, cur ? ALPHA(t->accent, 200) : ALPHA(t->panel_alt, 150)); draw_text(&g_font_small, ix + 24, dy + i * 20 + 2, cur ? t->sel_text : t->text, lang_names[i]); }
    if (sel_settings == O_SAVER || sel_settings == O_SCAN) {
        settings_t *s = settings_get();
        float f = sel_settings == O_SAVER ? s->screensaver_min / 30.0f : (s->scan_delay_ms - 250) / 4750.0f;
        draw_rect_round(ix + 14, dy + 4, iw - 28, 6, 3, ALPHA(t->panel_alt, 255));
        if (f > 0) draw_rect_round_grad(ix + 14, dy + 4, (iw - 28) * f, 6, 3, col_lighten(t->accent, 0.2f), t->accent);
        draw_circle(ix + 14 + (iw - 28) * f, dy + 7, 5, t->text_hi);
    }
    if (sel_settings >= O_CHAT && sel_settings <= O_CONNECT) { net_status_t ns = net_status(); char l[64]; snprintf(l, sizeof l, "%s %s", ns.iface[0] ? ns.iface : "-", ns.ip[0] ? ns.ip : ""); draw_text(&g_font_small, ix + 14, dy, t->text_dim, l); }
    draw_clear_clip();
    draw_bottombar(tr(S_HINT_SETTINGS));
    osk_draw();
}

/* ---------- sysinfo ---------- */
typedef struct { str_id k; const char *v; } kv_t;
static void card(float x, float y, float w, float h, const char *title, const kv_t *rows, int n, int status) {
    theme_t *t = &g_theme;
    panel(x, y, w, h);
    section_label(x + 14, y + 12, title);
    if (status >= 0) { uint32_t c = status ? t->badge_e : t->badge_j; draw_glow(x + w - 22, y + 19, 12, ALPHA(c, 160)); draw_circle(x + w - 22, y + 19, 4, c); }
    for (int i = 0; i < n; i++) {
        float ry = y + 36 + i * 20;
        draw_text(&g_font_small, x + 14, ry + 1, t->text_dim, tr(rows[i].k));
        draw_text_right_clipped(&g_font_small, x + w - 14, ry + 1, w - 28 - text_width(&g_font_small, tr(rows[i].k)) - 8, t->text, rows[i].v);
        if (i < n - 1) draw_rect(x + 14, ry + 17, w - 28, 1, ALPHA(t->text_dim, 30));
    }
}
static void sysinfo_draw(void) {
    theme_t *t = &g_theme;
    draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr();
    draw_topbar(tr(S_SYSINFO), NULL); draw_topbar_status();
    char fw[32], bl[32], n[16], cnt[16], favs[16], ipl[40];
    if (sys.gdemu.valid) { snprintf(fw, sizeof fw, "%d.%d.%d", sys.gdemu.fw_major, sys.gdemu.fw_minor, sys.gdemu.fw_patch); snprintf(bl, sizeof bl, "%d.%d", (int)((sys.gdemu.bootloader >> 24) & 0xFF), (int)((sys.gdemu.bootloader >> 16) & 0xFF)); }
    else { strcpy(fw, tr(S_NO_GDEMU)); strcpy(bl, "-"); }
    snprintf(n, sizeof n, "%lu", (unsigned long)settings_get()->launches);
    snprintf(cnt, sizeof cnt, "%d", g_list.count);
    int nf = 0; for (int i = 0; i < g_list.count; i++) nf += g_list.items[i].favorite; snprintf(favs, sizeof favs, "%d", nf);
    net_status_t ns = net_status(); snprintf(ipl, sizeof ipl, "%s %s", ns.state == NET_UP ? ns.iface : tr(S_OFFLINE), ns.ip);
    const char *lang = sys.lang[0] ? sys.lang : "-", *bios = sys.bios_version[0] ? sys.bios_version : "-";
    const kv_t gd[] = { { S_GDEMU_FW, fw }, { S_GDEMU_BOOT, bl } };
    const kv_t con[] = { { S_BIOS, bios }, { S_SYS_REGION, sys.region }, { S_BROADCAST, sys.broadcast }, { S_CABLE, sys.cable }, { S_LANGUAGE, lang }, { S_NETWORK, ipl } };
    const kv_t st[] = { { S_GAMES, cnt }, { S_FAVORITES, favs }, { S_LAUNCHES, n } };
    float y = LIST_Y + enter_dy(), gap = 12, cw = (SCREEN_W - 2 * SAFE_X - gap) / 2;
    card(SAFE_X, y, cw, 90, tr(S_SEC_GDEMU), gd, 2, sys.gdemu.valid ? 1 : 0);
    card(SAFE_X, y + 90 + gap, cw, 108, tr(S_SEC_STATS), st, 3, -1);
    card(SAFE_X + cw + gap, y, cw, 210, tr(S_SEC_CONSOLE), con, 6, -1);
    float ay = y + 210 + gap, ah = LIST_H - 210 - gap;
    panel(SAFE_X, ay, SCREEN_W - 2 * SAFE_X, ah);
    draw_logo(SAFE_X + 14, ay + 14, 40, 255);
    float lx = SAFE_X + 68;
    draw_text(&g_font_big, lx, ay + 10, t->text_dim, "Cast Menu");
    draw_text(&g_font_small, lx + text_width(&g_font_big, "Cast Menu") + 12, ay + 20, t->accent, "v" CAST_VERSION);
    draw_text_clipped(&g_font_small, lx, ay + 38, SCREEN_W - 2 * SAFE_X - 80, t->text_dim, "Menu aberto para GDEMU. MIT. github.com/hudsonbrendon/castmenu");
    draw_text_clipped(&g_font_small, lx, ay + 54, SCREEN_W - 2 * SAFE_X - 80, ALPHA(t->text_dim, 170), "neuroacid (GDMENU) \xb7 Deunan (GDEMU) \xb7 KallistiOS \xb7 megavolt85 \xb7 mrneo240");
    float tx = lx;
    const char *tags[] = { "KallistiOS 2.3", "SH-4", "PVR", "GDEMU 5.13+", "BBA / DreamPi" };
    for (int i = 0; i < 5; i++) { float w = text_width(&g_font_small, tags[i]) + 14; if (tx + w > SCREEN_W - SAFE_X - 14) break; draw_rect_round(tx, ay + 76, w, 18, 9, ALPHA(t->panel_alt, 255)); draw_rect_round_outline(tx, ay + 76, w, 18, 9, ALPHA(t->accent, 80)); draw_text(&g_font_small, tx + 7, ay + 78, t->accent, tags[i]); tx += w + 8; }
    if (sys.gdemu.valid && (sys.gdemu.fw_major < 5 || (sys.gdemu.fw_major == 5 && sys.gdemu.fw_minor < 13))) {
        float ww = text_width(&g_font_small, tr(S_NEED_FW)) + 24;
        draw_rect_round((SCREEN_W - ww) / 2, BOTTOM_Y - 32, ww, 22, 11, ALPHA(t->badge_j, 230));
        draw_text_center(&g_font_small, SCREEN_W / 2, BOTTOM_Y - 28, 0xFFFFFFFF, tr(S_NEED_FW));
    }
    draw_bottombar(tr(S_HINT_SYSINFO));
}

/* ---------- dialogs ---------- */
static void dialog_exit_draw(void) {
    dialog_box(tr(S_EXIT_Q), 380, 130);
    const char *opts[2] = { tr(S_YES), tr(S_NO) };
    float y = (SCREEN_H - 130) / 2;
    for (int i = 0; i < 2; i++) pill_button(SCREEN_W / 2 - 120 + i * 130, y + 56, 110, i == sel_dialog, opts[i]);
    draw_hints(SCREEN_W / 2, y + 112, tr(S_HINT_DIALOG), 160);
}
static void dialog_exit_update(void) {
    if (in.pressed & (BTN_LEFT | BTN_RIGHT)) sel_dialog ^= 1;
    if (in.pressed & BTN_B) go(prev_state == ST_SETTINGS ? ST_SETTINGS : ST_BROWSE);
    if (in.pressed & BTN_A) { if (sel_dialog == 0) { settings_save(); exit_flag = 1; } else go(prev_state == ST_SETTINGS ? ST_SETTINGS : ST_BROWSE); }
}
static void dialog_disc_draw(void) {
    theme_t *t = &g_theme;
    float h = 92 + disc_set_n * 34;
    dialog_box(tr(S_CHOOSE_DISC), 360, h);
    float y0 = (SCREEN_H - h) / 2 + 48;
    for (int i = 0; i < disc_set_n; i++) {
        float bx = (SCREEN_W - 320) / 2, by = y0 + i * 34; int sel = i == sel_dialog;
        if (sel) { draw_rect_round_grad(bx, by, 320, 28, 7, col_lighten(t->sel_bg, 0.18f), t->sel_bg); draw_rect_round_outline(bx, by, 320, 28, 7, ALPHA(0xFFFFFF, 60)); }
        else draw_rect_round(bx, by, 320, 28, 7, ALPHA(t->panel_alt, 200));
        draw_circle(bx + 18, by + 14, 9, sel ? ALPHA(t->sel_text, 70) : t->accent);
        char dn[2] = { disc_set[i]->disc[0], 0 }; draw_text_center(&g_font_small, bx + 18, by + 6, t->sel_text, dn);
        char s[80]; snprintf(s, sizeof s, "%s %c / %c", tr(S_DISC), disc_set[i]->disc[0], disc_set[i]->disc[2]);
        draw_text(&g_font, bx + 36, by + 5, sel ? t->sel_text : t->text, s);
        char f[16]; snprintf(f, sizeof f, "%02d", disc_set[i]->slot);
        draw_text_right(&g_font_small, bx + 308, by + 8, sel ? ALPHA(t->sel_text, 180) : t->text_dim, f);
    }
    draw_hints(SCREEN_W / 2, (SCREEN_H - h) / 2 + h - 18, tr(S_HINT_DISC), 160);
}
static void dialog_disc_update(void) {
    if (in.repeat & BTN_UP) sel_dialog = (sel_dialog + disc_set_n - 1) % disc_set_n;
    if (in.repeat & BTN_DOWN) sel_dialog = (sel_dialog + 1) % disc_set_n;
    if (in.pressed & BTN_B) go(ST_BROWSE);
    if (in.pressed & BTN_A) { launch_target = disc_set[sel_dialog]; launch_fade = 0; go(ST_LAUNCH); }
}
static const char LETTERS[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static void letter_draw(void) {
    theme_t *t = &g_theme;
    dialog_box(tr(S_GOTO_LETTER), 440, 140);
    float x0 = (SCREEN_W - 27 * 15) / 2, y = (SCREEN_H - 140) / 2 + 56;
    for (int i = 0; i < 27; i++) {
        char s[2] = { LETTERS[i], 0 };
        int active = g_list.filter_letter == LETTERS[i];
        if (i == letter_idx) { draw_glow(x0 + i * 15 + 7, y + 9, 18, t->glow); draw_rect_round(x0 + i * 15 - 2, y - 3, 18, 24, 5, t->sel_bg); }
        else if (active) draw_rect_round_outline(x0 + i * 15 - 2, y - 3, 18, 24, 5, t->accent);
        draw_text_center(&g_font, x0 + i * 15 + 7, y, i == letter_idx ? t->sel_text : active ? t->accent : t->text, s);
    }
    draw_hints(SCREEN_W / 2, (SCREEN_H - 140) / 2 + 122, tr(S_HINT_LETTER), 160);
}
static void letter_update(void) {
    if (in.repeat & BTN_LEFT) letter_idx = (letter_idx + 26) % 27;
    if (in.repeat & BTN_RIGHT) letter_idx = (letter_idx + 1) % 27;
    if (in.pressed & BTN_B) go(ST_BROWSE);
    if (in.pressed & BTN_X) { g_list.filter_letter = g_list.filter_letter == LETTERS[letter_idx] ? 0 : LETTERS[letter_idx]; list_rebuild_view(); cursor = 0; scroll_target = 0; cf_pos = 0; go(ST_BROWSE); }
    if (in.pressed & BTN_A) {
        for (int i = 0; i < g_list.view_count; i++) {
            char c = list_view_item(i)->name[0]; if (c >= 'a' && c <= 'z') c -= 32;
            int hit = LETTERS[letter_idx] == '#' ? !(c >= 'A' && c <= 'Z') : c == LETTERS[letter_idx];
            if (hit) { cursor = i; scroll_target = clampf((i - LIST_ROWS / 2) * ROW_H, 0, (g_list.view_count - LIST_ROWS) * ROW_H); cf_pos = i - 2; break; }
        }
        go(ST_BROWSE);
    }
}

/* ---------- launch / saver ---------- */
static void launch_draw(void) {
    theme_t *t = &g_theme;
    browse_draw();
    float f = clampf(launch_fade, 0, 1), a = clampf((launch_fade - 0.35f) * 1.8f, 0, 1);
    draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(235 * f)));
    if (a <= 0) return;
    float cs = 150 + 20 * ease_out(a);
    draw_glow(SCREEN_W / 2, 190, 170, col_scale_alpha(t->glow, a));
    draw_cover(launch_target, SCREEN_W / 2 - cs / 2, 190 - cs / 2, cs, 1);
    draw_text_center(&g_font_big, SCREEN_W / 2, 330, ALPHA(t->text_hi, (int)(255 * a)), launch_target->name);
    char dots[8]; int nd = ((int)(ui_t * 3)) % 4; snprintf(dots, sizeof dots, "%.*s", nd, "...");
    char s[48]; snprintf(s, sizeof s, "%s%s", tr(S_LAUNCHING), dots);
    draw_text_spaced(&g_font_small, SCREEN_W / 2 - text_width_spaced(&g_font_small, 2, tr(S_LAUNCHING)) / 2, 364, 2, ALPHA(t->text_dim, (int)(255 * a)), s);
    float bw = 220, bx = (SCREEN_W - bw) / 2, by = 390, p = clampf(launch_fade / 1.4f, 0, 1);
    draw_rect_round(bx, by, bw, 6, 3, ALPHA(t->panel_alt, (int)(255 * a)));
    draw_rect_round_grad(bx, by, bw * p, 6, 3, col_lighten(t->accent, 0.2f), t->accent);
    draw_glow(bx + bw * p, by + 3, 14, col_scale_alpha(t->glow, a));
}
static void saver_draw(void) {
    draw_frame_begin(); draw_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF000000); draw_frame_tr();
    float x = 200 + 180 * sinf(ui_t * 0.37f), y = 200 + 150 * cosf(ui_t * 0.23f);
    draw_wordmark(x, y, 120);
}

/* ---------- public ---------- */
static void scan_avatars(void) {
    n_avatars = 0;
    file_t d = fs_open("/cd/avatars", O_RDONLY | O_DIR); if (d < 0) return;
    const dirent_t *e;
    while ((e = fs_readdir(d)) && n_avatars < 16) {
        const char *dot = strrchr(e->name, '.'); if (!dot || strcasecmp(dot, ".tex")) continue;
        int l = dot - e->name; if (l >= AVATAR_LEN) l = AVATAR_LEN - 1;
        memcpy(avatars[n_avatars], e->name, l); avatars[n_avatars][l] = 0;
        for (char *p = avatars[n_avatars]; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        n_avatars++;
    }
    fs_close(d);
}
void ui_init(void) {
    draw_init(); input_init(); covers_init(); voice_init();
    settings_t *s = settings_get();
    theme_scan(); if (s->theme >= theme_count()) s->theme = 0;
    if (s->view > 2) s->view = 0; if (s->lang >= LANG_COUNT) s->lang = 0;
    font_load("/rd/font/ui", &g_font); font_load("/rd/font/ui_big", &g_font_big); font_load("/rd/font/ui_small", &g_font_small);
    if (font_load("/rd/font/ui_display", &g_font_display)) g_font_display = g_font_big;
    theme_load(s->theme);
    scan_avatars();
    sysinfo_collect(&sys);
    last_t = timer_ms_gettime64();
    state = ST_BOOT; fade = 1;
}

int ui_wants_exit(void) { return exit_flag; }

static void launch_from(const game_t *g) { launch_target = g; launch_fade = 0; go(ST_LAUNCH); }

void ui_frame(void) {
    uint64_t now = timer_ms_gettime64();
    ui_dt = (now - last_t) / 1000.0f; if (ui_dt > 0.1f) ui_dt = 0.1f; last_t = now; ui_t += ui_dt;
    input_poll(&in);
    settings_t *s = settings_get();
    if (state == ST_BOOT) {
        g_list.sort = s->sort; g_list.hide_multidisc_extra = s->hide_multidisc; list_set_category(CAT_ALL);
        int i = list_view_index_of_slot(s->last_slot); cursor = i >= 0 ? i : 0; cf_pos = cursor;
        scroll_target = clampf((cursor - LIST_ROWS / 2) * ROW_H, 0, (g_list.view_count - LIST_ROWS) * ROW_H); scroll_y = scroll_target;
        if (s->net_auto) net_start();
        go(ST_BROWSE); cat_anim = 1;
    }
    chat_background_poll(); voice_update();
    if (s->screensaver_min && state == ST_BROWSE && in.idle_ms > (uint32_t)s->screensaver_min * 60000u) go(ST_SAVER);
    int act;
    switch (state) {
    case ST_BROWSE: browse_update(); break;
    case ST_DETAILS: act = details_update(&in); if (act == ACT_CLOSE) go(ST_BROWSE); else if (act == ACT_LAUNCH) launch_from(details_game()); break;
    case ST_SIDEBAR: act = sidebar_update(&in); if (act == ACT_CLOSE) go(ST_BROWSE); else if (act == ACT_LAUNCH) launch_from(sidebar_game()); break;
    case ST_SETTINGS: settings_update(); break;
    case ST_SYSINFO: if (in.pressed & (BTN_B | BTN_A | BTN_START)) go(prev_state == ST_SETTINGS ? ST_SETTINGS : ST_BROWSE); break;
    case ST_CHAT: if (chat_update(&in) == ACT_CLOSE) go(ST_SETTINGS); break;
    case ST_AI: if (ai_update(&in) == ACT_CLOSE) go(ST_SETTINGS); break;
    case ST_DIALOG_EXIT: dialog_exit_update(); break;
    case ST_DIALOG_DISC: dialog_disc_update(); break;
    case ST_LETTER: letter_update(); break;
    case ST_SAVER: if (in.pressed) go(ST_BROWSE); break;
    case ST_LAUNCH: launch_fade += ui_dt * 2.0f; if (launch_fade > 1.5f) launch_game(launch_target); break;
    default: break;
    }
    scroll_y = lerp_to(scroll_y, scroll_target, 14, ui_dt);
    set_scroll = lerp_to(set_scroll, set_scroll_target, 14, ui_dt);
    sel_anim = clampf(sel_anim + ui_dt * 5, 0, 1);
    ui_enter = clampf(ui_enter + ui_dt * 4, 0, 1);
    cat_anim = clampf(cat_anim + ui_dt * 4, 0, 1);
    fade = clampf(fade - ui_dt * 1.5f, 0, 1);
    toast_active();

    switch (state) {
    case ST_SETTINGS: settings_draw(); break;
    case ST_SYSINFO: sysinfo_draw(); break;
    case ST_LAUNCH: launch_draw(); break;
    case ST_SAVER: saver_draw(); break;
    case ST_CHAT: draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr(); chat_draw(); break;
    case ST_AI: draw_frame_begin(); draw_background(); draw_frame_tr(); draw_background_tr(); ai_draw(); break;
    case ST_DETAILS: { float keep = ui_enter; ui_enter = 1; browse_draw(); ui_enter = keep; details_draw(); } break;
    case ST_SIDEBAR: { float keep = ui_enter; ui_enter = 1; browse_draw(); ui_enter = keep; sidebar_draw(1); } break;
    case ST_DIALOG_EXIT: if (prev_state == ST_SETTINGS) settings_draw(); else browse_draw(); dialog_exit_draw(); break;
    case ST_DIALOG_DISC: browse_draw(); dialog_disc_draw(); break;
    case ST_LETTER: browse_draw(); letter_draw(); break;
    default: browse_draw(); break;
    }
    toast_draw();
    if (fade > 0) draw_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * fade)));
    draw_frame_end();
}
