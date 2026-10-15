#include "lang.h"
#include "../settings.h"
int lang_current(void) { int l = settings_get()->lang; return l >= 0 && l < LANG_COUNT ? l : 0; }
const char *tr(str_id id) { return lang_table[lang_current()][id]; }
