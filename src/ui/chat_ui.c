/* Cast Chat: public rooms, friends list, private text + voice messages. */
#include "screens.h"
#include "chrome.h"
#include "osk.h"
#include "anim.h"
#include "../settings.h"
#include "../net/netif.h"
#include "../net/proto.h"
#include "../voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONV_MAX 10
#define MSG_MAX 40
typedef struct { char from[24]; char text[PROTO_TEXT_MAX]; uint8_t *voice; int voice_len; uint8_t mine, is_voice; } msg_t;
typedef struct { char name[32]; int is_room; msg_t m[MSG_MAX]; int n, unread; } conv_t;
static conv_t convs[CONV_MAX]; static int nconv, cur_conv = -1;
static char rooms[16][32]; static int room_users[16], nrooms;
static char friends[32][24]; static int friend_on[32], nfriends;
static int tab, sel, welcomed; static char motd[96]; static char compose[PROTO_TEXT_MAX]; static char add_nick[24];
static int osk_mode; /* 1 compose, 2 add friend */
static uint32_t last_ping, ping_sent; static float connect_retry;
static int unread_total, recording_was;

/* ---------- connection shared with Cast AI ---------- */
static void parse_server(char *host, int cap, int *port) {
    const char *s = settings_get()->server; const char *c = strrchr(s, ':');
    int hl = c ? (int)(c - s) : (int)strlen(s); if (hl > cap - 1) hl = cap - 1;
    memcpy(host, s, hl); host[hl] = 0; *port = c ? atoi(c + 1) : 7350;
}
void chatnet_ensure(void) {
    net_status_t ns = net_status();
    if (ns.state == NET_IDLE || ns.state == NET_FAILED) net_start();
    if (ns.state == NET_UP && !proto_connected() && connect_retry <= 0) {
        char host[40]; int port; parse_server(host, sizeof host, &port);
        welcomed = 0;
        if (proto_connect(host, port, settings_get()->nick, lang_codes[lang_current()]) < 0) connect_retry = 5;
    }
}
int chatnet_online(void) { return proto_connected() && welcomed; }

static conv_t *conv_get(const char *name, int is_room, int create) {
    for (int i = 0; i < nconv; i++) if (!strcmp(convs[i].name, name)) return &convs[i];
    if (!create) return NULL;
    conv_t *c = nconv < CONV_MAX ? &convs[nconv++] : &convs[CONV_MAX - 1];
    for (int i = 0; i < c->n; i++) free(c->m[i].voice);
    memset(c, 0, sizeof *c); strncpy(c->name, name, sizeof c->name - 1); c->is_room = is_room;
    return c;
}
static void conv_push(conv_t *c, const char *from, const char *text, int mine, uint8_t *voice, int vlen) {
    if (c->n == MSG_MAX) { free(c->m[0].voice); memmove(c->m, c->m + 1, sizeof(msg_t) * (MSG_MAX - 1)); c->n--; }
    msg_t *m = &c->m[c->n++]; memset(m, 0, sizeof *m);
    strncpy(m->from, from, sizeof m->from - 1); strncpy(m->text, text, sizeof m->text - 1);
    m->mine = mine; m->voice = voice; m->voice_len = vlen; m->is_voice = voice != NULL;
}
static void parse_list(const char *s, int room) {
    char buf[PROTO_TEXT_MAX]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    if (room) nrooms = 0; else nfriends = 0;
    for (char *p = strtok(buf, " "); p; p = strtok(NULL, " ")) {
        char *c = strrchr(p, ':'); int v = 0; if (c) { *c = 0; v = atoi(c + 1); }
        if (room && nrooms < 16) { strncpy(rooms[nrooms], p, 31); room_users[nrooms++] = v; }
        else if (!room && nfriends < 32) { strncpy(friends[nfriends], p, 23); friend_on[nfriends++] = v; }
    }
}

void chat_background_poll(void) {
    if (connect_retry > 0) connect_retry -= ui_dt;
    if (!proto_connected()) return;
    uint32_t now = proto_now_ms();
    if (welcomed && now - last_ping > 5000) { last_ping = now; ping_sent = now; proto_send("PING %lu", (unsigned long)now); }
    proto_event_t ev;
    while (proto_poll(&ev)) {
        switch (ev.type) {
        case PEV_WELCOME: welcomed = 1; strncpy(motd, ev.text, sizeof motd - 1); proto_send("ROOMS"); proto_send("FRIENDS"); break;
        case PEV_PONG: net_set_ping((int)(now - ping_sent)); break;
        case PEV_ROOMS: parse_list(ev.text, 1); break;
        case PEV_FRIENDS: parse_list(ev.text, 0); break;
        case PEV_PRESENCE: for (int i = 0; i < nfriends; i++) if (!strcmp(friends[i], ev.a)) friend_on[i] = atoi(ev.text); break;
        case PEV_JOINED: { conv_t *c = conv_get(ev.a, 1, 1); cur_conv = c - convs; proto_send("HISTORY %s", ev.a); } break;
        case PEV_MSG: {
            char *sep = strchr(ev.text, '\x01'); const char *from = ev.text, *text = ""; if (sep) { *sep = 0; text = sep + 1; }
            conv_t *c = conv_get(ev.a, 1, 1); int mine = !strcmp(from, settings_get()->nick);
            conv_push(c, from, text, mine, NULL, 0); if (!mine && c - convs != cur_conv) { c->unread++; unread_total++; }
        } break;
        case PEV_PM: {
            conv_t *c = conv_get(ev.a, 0, 1); conv_push(c, ev.a, ev.text, 0, NULL, 0);
            if (c - convs != cur_conv) { c->unread++; unread_total++; char t[80]; snprintf(t, sizeof t, "%s: %s", ev.a, ev.text); show_toast(t, 0); }
        } break;
        case PEV_VOICE: {
            conv_t *c = conv_get(ev.a, 0, 1); char d[32]; snprintf(d, sizeof d, "%s (%ds)", tr(S_VOICE_MSG), ev.blob_len / VOICE_RATE);
            conv_push(c, ev.a, d, 0, ev.blob, ev.blob_len);
            if (c - convs == cur_conv) voice_play(ev.blob, ev.blob_len); else { c->unread++; unread_total++; char t[80]; snprintf(t, sizeof t, "%s: %s", ev.a, tr(S_VOICE_MSG)); show_toast(t, 0); }
        } break;
        case PEV_AI: ai_on_reply(ev.text); break;
        case PEV_ERR: show_toast(ev.text, 3); break;
        case PEV_DISCONNECTED: welcomed = 0; connect_retry = 5; net_set_ping(-1); break;
        default: break;
        }
    }
}
int chat_unread(void) { return unread_total; }

void chat_open(void) { chatnet_ensure(); tab = 0; sel = 0; if (chatnet_online()) { proto_send("ROOMS"); proto_send("FRIENDS"); } }

static void open_selected(void) {
    if (tab == 0) { if (sel < nrooms) proto_send("JOIN %s", rooms[sel]); }
    else {
        if (sel == 0) { add_nick[0] = 0; osk_mode = 2; osk_open(tr(S_ADD_FRIEND), add_nick, sizeof add_nick); return; }
        int f = sel - 1; if (f >= nfriends) return;
        conv_t *c = conv_get(friends[f], 0, 1); cur_conv = c - convs; unread_total -= c->unread; c->unread = 0;
        proto_send("HISTORY %s", friends[f]);
    }
}
static void send_compose(void) {
    if (cur_conv < 0 || !compose[0]) return;
    conv_t *c = &convs[cur_conv];
    if (c->is_room) proto_send("SAY %s %s", c->name, compose);
    else { proto_send("PM %s %s", c->name, compose); conv_push(c, settings_get()->nick, compose, 1, NULL, 0); }
    compose[0] = 0;
}

int chat_update(const input_t *in) {
    chatnet_ensure();
    if (osk_active()) {
        int r = osk_update(in);
        if (r == 1) { if (osk_mode == 1) send_compose(); else if (osk_mode == 2 && add_nick[0]) proto_send("ADD %s", add_nick); }
        return ACT_NONE;
    }
    /* voice: hold Y */
    int can_voice = cur_conv >= 0 && chatnet_online();
    if ((in->pressed & BTN_Y) && can_voice) { if (voice_record_start()) show_toast(tr(S_NO_MIC), 3); }
    if ((in->released & BTN_Y) && voice_recording()) {
        uint8_t *d; int l; voice_record_stop(&d, &l);
        if (l > VOICE_RATE / 4) {
            conv_t *c = &convs[cur_conv];
            proto_send_blob("VOICE", c->name, d, l);
            char t[32]; snprintf(t, sizeof t, "%s (%ds)", tr(S_VOICE_MSG), l / VOICE_RATE);
            uint8_t *copy = malloc(l); if (copy) memcpy(copy, d, l);
            conv_push(c, settings_get()->nick, t, 1, copy, l); show_toast(tr(S_SENT), 2);
        }
    }
    int count = tab == 0 ? nrooms : nfriends + 1;
    if (in->repeat & BTN_UP && count) sel = (sel + count - 1) % count;
    if (in->repeat & BTN_DOWN && count) sel = (sel + 1) % count;
    if (in->pressed & (BTN_L | BTN_R)) { tab ^= 1; sel = 0; }
    if (in->pressed & BTN_A) open_selected();
    if ((in->pressed & BTN_X) && cur_conv >= 0) { osk_mode = 1; osk_open(tr(S_TYPE_HERE), compose, sizeof compose); }
    if (in->pressed & BTN_B) return ACT_CLOSE;
    return ACT_NONE;
}

static void status_line(float rx, float y) {
    theme_t *t = &g_theme;
    net_status_t ns = net_status();
    const char *s = chatnet_online() ? tr(S_CONNECTED) : (ns.state == NET_CONNECTING || (ns.state == NET_UP && !chatnet_online())) ? tr(S_CONNECTING) : ns.state == NET_NO_ADAPTER ? tr(S_NO_ADAPTER) : tr(S_OFFLINE);
    uint32_t c = chatnet_online() ? t->badge_e : ns.state == NET_CONNECTING ? t->badge_gd : t->badge_j;
    float w = text_width(&g_font_small, s);
    draw_text(&g_font_small, rx - w, y, t->text, s);
    draw_glow(rx - w - 14, y + 7, 12, ALPHA(c, 180)); draw_circle(rx - w - 14, y + 7, 4, c);
    if (chatnet_online() && ns.ping_ms >= 0) { char p[24]; snprintf(p, sizeof p, "%s %d ms", tr(S_PING), ns.ping_ms); draw_text(&g_font_small, rx - w - 36 - text_width(&g_font_small, p), y, t->text_dim, p); }
}

void chat_draw(void) {
    theme_t *t = &g_theme;
    draw_rect_grad(0, 0, SCREEN_W, 64, ALPHA(t->bg, 200), ALPHA(t->bg, 0));
    draw_logo(SAFE_X, SAFE_TOP - 6, 32, 255);
    draw_text_shadow(&g_font_big, SAFE_X + 42, SAFE_TOP, t->text_hi, ALPHA(0x000000, 90), "Cast");
    draw_text_shadow(&g_font_big, SAFE_X + 42 + text_width(&g_font_big, "Cast") + 6, SAFE_TOP, t->accent, ALPHA(0x000000, 90), "Chat");
    status_line(SCREEN_W - SAFE_X, SAFE_TOP + 6);
    float dy = enter_dy();
    /* left: tabs + list */
    float lx = SAFE_X, ly = CONTENT_Y + dy, lw = 190, lh = CONTENT_H;
    panel(lx, ly, lw, lh);
    const char *tabs[2] = { tr(S_ROOMS), tr(S_FRIENDS) };
    for (int i = 0; i < 2; i++) {
        float tx = lx + 10 + i * (lw - 20) / 2, tw = (lw - 20) / 2 - 4;
        if (i == tab) { draw_rect_round_grad(tx, ly + 10, tw, 24, 6, col_lighten(t->accent, 0.15f), t->accent); draw_text_center(&g_font_small, tx + tw / 2, ly + 15, t->sel_text, tabs[i]); }
        else { draw_rect_round(tx, ly + 10, tw, 24, 6, ALPHA(t->panel_alt, 255)); draw_text_center(&g_font_small, tx + tw / 2, ly + 15, t->text_dim, tabs[i]); }
    }
    draw_set_clip((int)lx + 2, (int)ly + 42, (int)lw - 4, (int)lh - 50);
    int count = tab == 0 ? nrooms : nfriends + 1;
    for (int i = 0; i < count && i < 11; i++) {
        float y = ly + 44 + i * 29; int is = i == sel;
        if (is) { draw_rect_round_grad(lx + 8, y, lw - 16, 27, 6, col_lighten(t->sel_bg, 0.15f), t->sel_bg); draw_rect_round_outline(lx + 8, y, lw - 16, 27, 6, ALPHA(0xFFFFFF, 60)); }
        uint32_t fg = is ? t->sel_text : t->text;
        if (tab == 0) {
            draw_text(&g_font_small, lx + 18, y + 7, is ? ALPHA(t->sel_text, 180) : t->accent, "#");
            draw_text_clipped(&g_font, lx + 30, y + 4, lw - 80, fg, rooms[i]);
            char u[8]; snprintf(u, sizeof u, "%d", room_users[i]); draw_text_right(&g_font_small, lx + lw - 18, y + 7, is ? ALPHA(t->sel_text, 180) : t->text_dim, u);
        } else if (i == 0) {
            draw_circle(lx + 24, y + 13, 8, is ? ALPHA(t->sel_text, 60) : ALPHA(t->accent, 120)); draw_text_center(&g_font_small, lx + 24, y + 6, fg, "+");
            draw_text(&g_font_small, lx + 40, y + 7, is ? t->sel_text : t->text_dim, tr(S_ADD_FRIEND));
        } else {
            int f = i - 1; char ini[2] = { friends[f][0], 0 }; if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] -= 32;
            draw_circle(lx + 24, y + 13, 9, is ? ALPHA(t->sel_text, 60) : t->accent_dim); draw_text_center(&g_font_small, lx + 24, y + 6, fg, ini);
            draw_circle(lx + 31, y + 20, 3.5f, friend_on[f] ? t->badge_e : ALPHA(t->text_dim, 150));
            draw_text_clipped(&g_font, lx + 42, y + 4, lw - 70, fg, friends[f]);
            conv_t *c = conv_get(friends[f], 0, 0);
            if (c && c->unread) { char u[8]; snprintf(u, sizeof u, "%d", c->unread); draw_circle(lx + lw - 22, y + 13, 8, t->badge_j); draw_text_center(&g_font_small, lx + lw - 22, y + 6, 0xFFFFFFFF, u); }
        }
    }
    if (tab == 0 && !nrooms) draw_text_wrap(&g_font_small, lx + 16, ly + 50, lw - 32, 15, t->text_dim, chatnet_online() ? "..." : tr(S_OFFLINE));
    draw_clear_clip();
    /* right: conversation */
    float rx = SAFE_X + 190 + 12, ry = CONTENT_Y + dy, rw = SCREEN_W - SAFE_X - rx, rh = CONTENT_H;
    panel(rx, ry, rw, rh);
    if (cur_conv < 0) {
        draw_text_center(&g_font, rx + rw / 2, ry + 150, t->text_dim, tr(S_SELECT_FRIEND));
        if (motd[0]) draw_text_wrap(&g_font_small, rx + 24, ry + 190, rw - 48, 15, ALPHA(t->text_dim, 160), motd);
    } else {
        conv_t *c = &convs[cur_conv];
        char title[48]; snprintf(title, sizeof title, "%s%s", c->is_room ? "# " : "", c->name);
        draw_text(&g_font, rx + 16, ry + 12, t->text_hi, title);
        draw_text_right(&g_font_small, rx + rw - 16, ry + 16, t->text_dim, c->is_room ? tr(S_ROOMS) : tr(S_PRIVATE));
        hairline(rx + 16, ry + 38, rw - 32);
        /* messages bottom-up */
        float bottom = ry + rh - 46, top = ry + 46;
        draw_set_clip((int)rx + 4, (int)top, (int)rw - 8, (int)(bottom - top));
        float y = bottom;
        for (int i = c->n - 1; i >= 0 && y > top - 60; i--) {
            msg_t *m = &c->m[i];
            int nl = draw_text_wrap(&g_font_small, 0, -1, rw - 110, 15, 0, m->text);
            float bh = nl * 15 + (c->is_room && !m->mine ? 26 : 12), bw = rw - 90;
            y -= bh + 8;
            float bx = m->mine ? rx + rw - 16 - bw : rx + 16;
            uint32_t bg = m->mine ? ALPHA(t->accent, 200) : m->is_voice ? ALPHA(t->accent2, 60) : ALPHA(t->panel_alt, 255);
            draw_rect_round(bx, y, bw, bh, 9, bg);
            if (m->is_voice) { draw_circle(bx + 16, y + bh / 2, 8, m->mine ? ALPHA(0xFFFFFF, 80) : t->accent); draw_text_center(&g_font_small, bx + 16, y + bh / 2 - 7, 0xFFFFFFFF, "\x10"); }
            float tx = bx + (m->is_voice ? 32 : 12), ty = y + 6;
            if (c->is_room && !m->mine) { draw_text(&g_font_small, tx, ty, t->accent2, m->from); ty += 14; }
            draw_text_wrap(&g_font_small, tx, ty, bw - 24 - (m->is_voice ? 20 : 0), 15, m->mine ? t->sel_text : t->text, m->text);
        }
        draw_clear_clip();
        draw_rect_grad(rx + 4, top, rw - 8, 12, ALPHA(t->panel, 255), ALPHA(t->panel, 0));
        /* compose bar */
        if (voice_recording()) {
            float f = (float)voice_record_len() / VOICE_MAX_BYTES;
            draw_rect_round(rx + 16, ry + rh - 40, rw - 32, 28, 14, ALPHA(t->badge_j, 60));
            draw_rect_round_grad(rx + 16, ry + rh - 40, (rw - 32) * f, 28, 14, col_lighten(t->badge_j, 0.2f), t->badge_j);
            draw_circle(rx + 32, ry + rh - 26, 6 + 2 * (0.5f + 0.5f * __builtin_sinf(ui_t * 8)), 0xFFFFFFFF);
            draw_text(&g_font_small, rx + 46, ry + rh - 33, t->text_hi, tr(S_RECORDING));
        } else draw_textbox(rx + 16, ry + rh - 40, rw - 32, 28, compose, tr(S_TYPE_HERE), 0);
    }
    draw_bottombar(tr(S_HINT_CHAT));
    osk_draw();
    (void)recording_was;
}
