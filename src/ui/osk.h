/* On-screen keyboard (controller) that also accepts the Dreamcast keyboard. */
#pragma once
#include "input.h"

void osk_open(const char *title, char *buf, int cap);
int  osk_active(void);
int  osk_update(const input_t *in);   /* 1 submitted, -1 cancelled, 0 still open */
void osk_draw(void);                  /* bottom half overlay */
