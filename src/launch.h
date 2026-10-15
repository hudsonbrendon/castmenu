/* Boot a game slot: switch GDEMU image, then hand over to the open loader
 * (src/loader) with the patch flags from settings. Never returns. */
#pragma once
#include "list.h"
void launch_game(const game_t *g) __attribute__((noreturn));
void launch_exit_to_bios(void) __attribute__((noreturn));
