/* Boot a game slot: switch GDEMU image, then hand over to the open loader
 * (src/loader) with the patch flags from settings. Never returns. */
#pragma once
#include "list.h"
void launch_game(const game_t *g) __attribute__((noreturn));
/* the same thing in stages, so the launch animation can hide the slow bits:
 * prepare = stop media + save settings (VMU write), mount = switch the GDEMU
 * image (the BIOS needs ~600 ms before the TOC is re-read), boot = loader. */
void launch_prepare(const game_t *g);
void launch_mount(const game_t *g);
void launch_boot(const game_t *g) __attribute__((noreturn));
void launch_exit_to_bios(void) __attribute__((noreturn));
int  launch_gdmenu_loader_present(void);   /* /cd/loader.bin shipped by the card tool */
