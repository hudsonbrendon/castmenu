#include "input.h"
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/keyboard.h>

static uint32_t prev, rep_btn; static uint64_t rep_t, last_act;
#define REP_DELAY 320
#define REP_RATE  60

void input_init(void) { last_act = timer_ms_gettime64(); }

int input_any_controller(void) { return maple_enum_type(0, MAPLE_FUNC_CONTROLLER) != NULL; }

void input_poll(input_t *in) {
    uint32_t b = 0; int jx = 0, jy = 0, lt = 0, rt = 0; in->connected = 0;
    for (int i = 0; i < 4; i++) {
        maple_device_t *d = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
        if (!d) continue;
        cont_state_t *st = maple_dev_status(d);
        if (!st) continue;
        in->connected = 1;
        if (st->buttons & CONT_A) b |= BTN_A;
        if (st->buttons & CONT_B) b |= BTN_B;
        if (st->buttons & CONT_X) b |= BTN_X;
        if (st->buttons & CONT_Y) b |= BTN_Y;
        if (st->buttons & CONT_START) b |= BTN_START;
        if (st->buttons & CONT_DPAD_UP) b |= BTN_UP;
        if (st->buttons & CONT_DPAD_DOWN) b |= BTN_DOWN;
        if (st->buttons & CONT_DPAD_LEFT) b |= BTN_LEFT;
        if (st->buttons & CONT_DPAD_RIGHT) b |= BTN_RIGHT;
        if (st->joyy < -64) b |= BTN_UP;
        if (st->joyy > 64) b |= BTN_DOWN;
        if (st->joyx < -64) b |= BTN_LEFT;
        if (st->joyx > 64) b |= BTN_RIGHT;
        if (st->ltrig > 100) b |= BTN_L;
        if (st->rtrig > 100) b |= BTN_R;
        jx = st->joyx; jy = st->joyy; lt = st->ltrig; rt = st->rtrig;
        break;
    }
    /* keyboard: arrows/enter/esc map to dpad/A/B */
    maple_device_t *kb = maple_enum_type(0, MAPLE_FUNC_KEYBOARD);
    in->kbd_char = 0;
    if (kb) {
        int ch = kbd_queue_pop(kb, 1); if (ch > 0 && ch < 256) in->kbd_char = ch;
        kbd_state_t *ks = maple_dev_status(kb);
        if (ks) {
#define KD(k) (ks->key_states[k].is_down)
            if (KD(KBD_KEY_UP)) b |= BTN_UP;
        if (KD(KBD_KEY_DOWN)) b |= BTN_DOWN;
            if (KD(KBD_KEY_LEFT)) b |= BTN_LEFT;
        if (KD(KBD_KEY_RIGHT)) b |= BTN_RIGHT;
            if (KD(KBD_KEY_ENTER)) b |= BTN_A;
        if (KD(KBD_KEY_ESCAPE) || KD(KBD_KEY_BACKSPACE)) b |= BTN_B;
            if (KD(KBD_KEY_TAB)) b |= BTN_X;
            if (KD(KBD_KEY_Y)) b |= BTN_Y;
            if (KD(KBD_KEY_SPACE)) b |= BTN_START;
        if (KD(KBD_KEY_PGUP)) b |= BTN_L;
        if (KD(KBD_KEY_PGDOWN)) b |= BTN_R;
#undef KD
            in->connected = 1;
        }
    }
    uint64_t now = timer_ms_gettime64();
    in->held = b; in->pressed = b & ~prev; in->released = prev & ~b; in->repeat = in->pressed;
    uint32_t dirs = b & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_L | BTN_R);
    if (dirs && dirs == rep_btn) {
        if (now - rep_t > REP_DELAY) { in->repeat |= dirs; rep_t = now - (REP_DELAY - REP_RATE); }
    } else { rep_btn = dirs; rep_t = now; }
    if (b) last_act = now;
    in->idle_ms = (uint32_t)(now - last_act);
    in->joyx = jx; in->joyy = jy; in->ltrig = lt; in->rtrig = rt;
    prev = b;
}
