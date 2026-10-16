/* VMU saves: list every memory card, copy a save to another card, delete it, back it up to the
 * Cast server (cloud) and restore from there. The GDEMU SD card cannot be written by the console. */
#pragma once
#include "input.h"
void vmu_ui_open(void);
int  vmu_ui_update(const input_t *in);   /* ACT_CLOSE to leave */
void vmu_ui_draw(void);
void vmu_ui_on_list(const char *text);                  /* VMULIST payload from the server */
void vmu_ui_on_data(const char *fn, const uint8_t *data, int len);   /* VMUDATA */
