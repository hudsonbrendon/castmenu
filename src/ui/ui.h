#pragma once
#include "../sysinfo.h"
void ui_init(void);
void ui_boot_progress(int pct, const char *msg);  /* called from list_load */
void ui_frame(void);                               /* one frame: input + draw */
int  ui_wants_exit(void);
