/* Cast AI: ask by text (on-screen keyboard) or voice (microphone). The server
 * relays to an LLM; see server/cast_server.py. */
#include "screens.h"
#include "chrome.h"
#include "osk.h"
#include "anim.h"
#include "../settings.h"
#include "../net/netif.h"
#include "../net/proto.h"
#include "../voice.h"
#include <stdio.h>
#include <string.h>

#define AI_MSGS 12
static struct { char text[PROTO_TEXT_MAX * 2]; uint8_t mine; } msgs[AI_MSGS]; static int nmsgs;
static char question[PROTO_TEXT_MAX]; static int waiting; static float scroll;

static void push(const char *t, int mine) {
    if (nmsgs == AI_MSGS) { memmove(msgs, msgs + 1, sizeof msgs[0] * (AI_MSGS - 1)); nmsgs--; }
    strncpy(msgs[nmsgs].text, t, sizeof msgs[0].text - 1); msgs[nmsgs].text[sizeof msgs[0].text - 1] = 0; msgs[nmsgs++].mine = mine;
}
void ai_on_reply(const char *text) { push(text, 0); waiting = 0; }
void ai_open(void) { chatnet_ensure(); if (!nmsgs) { char h[160]; snprintf(h, sizeof h, "%s, %s! %s", tr(S_WELCOME), settings_get()->nick, tr(S_AI_INTRO)); push(h, 0); } }

static void ask(void) {
    if (!question[0]) return;
    push(question, 1);
    if (chatnet_online()) { proto_send("AI %s", question); waiting = 1; } else show_toast(tr(S_OFFLINE), 3);
    question[0] = 0;
}

int ai_update(const input_t *in) {
    chatnet_ensure();
    if (osk_active()) { if (osk_update(in) == 1) ask(); return ACT_NONE; }
    if (in->pressed & BTN_Y) { if (!chatnet_online()) show_toast(tr(S_OFFLINE), 3); else if (voice_record_start()) show_toast(tr(S_NO_MIC), 3); }
    if ((in->released & BTN_Y) && voice_recording()) {
        uint8_t *d; int l; voice_record_stop(&d, &l);
        if (l > VOICE_RATE / 4) { char t[48]; snprintf(t, sizeof t, "%s (%ds)", tr(S_VOICE_MSG), l / VOICE_RATE); push(t, 1); proto_send_blob("AIVOICE", NULL, d, l); waiting = 1; }
    }
    if (in->pressed & BTN_X) osk_open(tr(S_ASK_HERE), question, sizeof question);
    if (in->pressed & BTN_A) { if (question[0]) ask(); else osk_open(tr(S_ASK_HERE), question, sizeof question); }
    if (in->pressed & BTN_B) return ACT_CLOSE;
    return ACT_NONE;
}

void ai_draw(void) {
    theme_t *t = &g_theme;
    draw_rect_grad(0, 0, SCREEN_W, 64, ALPHA(t->bg, 200), ALPHA(t->bg, 0));
    draw_logo(24, 12, 32, 255);
    draw_text_shadow(&g_font_big, 66, 16, t->text_hi, ALPHA(0x000000, 90), "Cast");
    draw_text_shadow(&g_font_big, 66 + text_width(&g_font_big, "Cast") + 6, 16, t->accent, ALPHA(0x000000, 90), "AI");
    /* status strip */
    net_status_t ns = net_status(); char srv[48]; strncpy(srv, settings_get()->server, sizeof srv - 1); srv[sizeof srv - 1] = 0;
    float sx = 200;
    draw_text(&g_font_small, sx, 26, t->text_dim, tr(S_SERVER)); draw_text(&g_font_small, sx + text_width(&g_font_small, tr(S_SERVER)) + 8, 26, t->text, srv);
    const char *st = chatnet_online() ? tr(S_CONNECTED) : ns.state == NET_CONNECTING || ns.state == NET_UP ? tr(S_CONNECTING) : tr(S_OFFLINE);
    char pg[24]; snprintf(pg, sizeof pg, "%s %d ms", tr(S_PING), ns.ping_ms < 0 ? 0 : ns.ping_ms);
    float rx = SCREEN_W - 24; float pw = text_width(&g_font_small, pg);
    draw_text(&g_font_small, rx - pw, 26, t->text_dim, pg);
    float stw = text_width(&g_font_small, st);
    draw_text(&g_font_small, rx - pw - 24 - stw, 26, chatnet_online() ? t->badge_e : t->badge_gd, st);
    draw_text(&g_font_small, rx - pw - 24 - stw - 8 - text_width(&g_font_small, tr(S_STATUS)), 26, t->text_dim, tr(S_STATUS));
    float dy = enter_dy();
    /* left info card */
    float lx = 24, ly = 64 + dy, lw = 190, lh = 366;
    panel(lx, ly, lw, lh);
    int nl = draw_text_wrap(&g_font, lx + 14, ly + 14, lw - 28, 19, t->text, tr(S_AI_INTRO));
    float y = ly + 14 + nl * 19 + 8;
    nl = draw_text_wrap(&g_font_small, lx + 14, y, lw - 28, 14, t->text_dim, tr(S_AI_WARN)); y += nl * 14 + 12;
    hairline(lx + 14, y, lw - 28); y += 10;
    section_label(lx + 14, y, tr(S_SHORTCUTS)); y += 26;
    const char *keys[4][2] = { { "Y", tr(S_VOICE_MSG) }, { "X", tr(S_ASK_HERE) }, { "A", tr(S_SEND) }, { "B", tr(S_BACK) } };
    for (int i = 0; i < 4; i++) { draw_button_glyph(lx + 14, y + 9, keys[i][0], 255); draw_text_clipped(&g_font_small, lx + 40, y + 2, lw - 54, t->text, keys[i][1]); y += 26; }
    /* spiral watermark */
    draw_ring(SCREEN_W - 90, 200, 60, ALPHA(t->accent, 25)); draw_ring(SCREEN_W - 90, 200, 40, ALPHA(t->accent, 20)); draw_ring(SCREEN_W - 90, 200, 20, ALPHA(t->accent, 15));
    /* conversation */
    float rx0 = 226, ry = 64 + dy, rw = SCREEN_W - 24 - rx0, rh = 300;
    panel(rx0, ry, rw, rh);
    draw_set_clip((int)rx0 + 4, (int)ry + 8, (int)rw - 8, (int)rh - 16);
    float yy = ry + rh - 12;
    if (waiting) { yy -= 22; draw_text(&g_font_small, rx0 + 20, yy, ALPHA(t->text_dim, 150 + (int)(100 * __builtin_sinf(ui_t * 4))), tr(S_THINKING)); yy -= 6; }
    for (int i = nmsgs - 1; i >= 0 && yy > ry; i--) {
        int lines = draw_text_wrap(&g_font, 0, -1, rw - 80, 19, 0, msgs[i].text);
        float bh = lines * 19 + 14, bw = rw - 60; yy -= bh + 8;
        float bx = msgs[i].mine ? rx0 + rw - 16 - bw : rx0 + 16;
        draw_rect_round(bx, yy, bw, bh, 10, msgs[i].mine ? ALPHA(t->accent, 200) : ALPHA(t->panel_alt, 230));
        draw_text_wrap(&g_font, bx + 12, yy + 7, bw - 24, 19, msgs[i].mine ? t->sel_text : t->text, msgs[i].text);
    }
    draw_clear_clip();
    draw_rect_grad(rx0 + 4, ry + 4, rw - 8, 14, ALPHA(t->panel, 255), ALPHA(t->panel, 0));
    /* input row */
    float iy = ry + rh + 10, bw2 = 70;
    if (voice_recording()) {
        float f = (float)voice_record_len() / VOICE_MAX_BYTES;
        draw_rect_round(rx0, iy, rw - bw2 - 10, 48, 10, ALPHA(t->badge_j, 60));
        draw_rect_round_grad(rx0, iy, (rw - bw2 - 10) * f, 48, 10, col_lighten(t->badge_j, 0.2f), t->badge_j);
        draw_text(&g_font_small, rx0 + 16, iy + 17, t->text_hi, tr(S_RECORDING));
    } else draw_textbox(rx0, iy, rw - bw2 - 10, 48, question, tr(S_ASK_HERE), 0);
    char cnt[16]; snprintf(cnt, sizeof cnt, "%d/%d", (int)strlen(question), PROTO_TEXT_MAX - 1);
    draw_text_right(&g_font_small, rx0 + rw - bw2 - 22, iy + 32, ALPHA(t->text_dim, 140), cnt);
    draw_rect_round_grad(rx0 + rw - bw2, iy, bw2, 48, 10, col_lighten(t->accent, 0.15f), t->accent);
    draw_text_center(&g_font_small, rx0 + rw - bw2 / 2, iy + 28, t->sel_text, tr(S_SEND));
    float tri[8] = { rx0 + rw - bw2 / 2 - 8, iy + 20, rx0 + rw - bw2 / 2 + 8, iy + 20, rx0 + rw - bw2 / 2, iy + 8, rx0 + rw - bw2 / 2, iy + 8 };
    draw_rect_pts(tri, t->sel_text, t->sel_text, t->sel_text, t->sel_text);
    (void)scroll;
    draw_bottombar(tr(S_HINT_AI));
    osk_draw();
}
