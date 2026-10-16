/* Per-game cheats: /cd/cheats/NNN.txt (card tool packs cheats.txt from the game folder or
 * cheats/<PRODUCT>.txt from the card root). Text format: blank-line separated blocks, first line
 * = name, then "XXXXXXXX YYYYYYYY" code lines. Enabled flags live in settings (8 per game).
 * The open loader applies constant-write codes (types 0/1/2) from a GD-ROM syscall hook. */
#pragma once
#include <stdint.h>
#define CHEAT_MAX 16
#define CHEAT_CODES 12
typedef struct { char name[40]; uint32_t code[CHEAT_CODES][2]; int n; } cheat_t;
int  cheats_load(uint16_t slot, cheat_t *out, int max);              /* number of cheats found */
int  cheats_enabled(uint16_t slot, int idx);
void cheats_toggle(uint16_t slot, int idx);
int  cheats_collect(uint16_t slot, uint32_t *pairs, int max_pairs);  /* enabled codes -> (code, value) list */
