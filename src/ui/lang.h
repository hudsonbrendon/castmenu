#pragma once
/* UI strings in 6 languages (pt, en, es, de, fr, it); table generated from
 * assets/lang/strings.tsv by tools/cast_card/mklang.py. Latin-1 text.
 * Hint strings: "BTN label|BTN label|..." — the bottom bar draws BTN as a glyph. */
#include "lang_ids.h"
extern const char *const lang_table[LANG_COUNT][S_COUNT];
extern const char *const lang_names[LANG_COUNT];
extern const char *const lang_codes[LANG_COUNT];
const char *tr(str_id id);
int lang_current(void);
