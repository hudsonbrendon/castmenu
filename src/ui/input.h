/* Controller input with edge detection, auto-repeat and analog->digital mapping. */
#pragma once
#include <stdint.h>

enum {
    BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_X = 1 << 2, BTN_Y = 1 << 3, BTN_START = 1 << 4,
    BTN_UP = 1 << 5, BTN_DOWN = 1 << 6, BTN_LEFT = 1 << 7, BTN_RIGHT = 1 << 8,
    BTN_L = 1 << 9, BTN_R = 1 << 10,
};

typedef struct {
    uint32_t held, pressed, released, repeat; /* repeat = pressed OR auto-repeat tick */
    int joyx, joyy, ltrig, rtrig;
    int connected;
    uint32_t idle_ms;                        /* time since last activity */
    int kbd_char;                            /* ASCII typed on a Dreamcast keyboard this frame, 0 if none */
} input_t;

void input_init(void);
void input_poll(input_t *out);               /* once per frame */
int  input_any_controller(void);
