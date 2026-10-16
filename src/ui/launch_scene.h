/* Launch animation: top view of a Dreamcast, lid opens, the game's disc
 * (cover art as label) floats in, seats and spins while a progress bar fills.
 * Timeline in seconds since the launch started; ui.c fires the real work at
 * the marks below so the slow steps hide behind the animation. */
#pragma once
#include "../list.h"
#define LAUNCH_T_SAVE  0.35f   /* screen is black: settings_save (VMU write) */
#define LAUNCH_T_MOUNT 2.20f   /* disc seated: switch the GDEMU image */
#define LAUNCH_T_BOOT  3.00f   /* bar full, >= 600 ms after mount: run the loader */
void launch_scene_begin(const game_t *g);               /* grabs the cover before /cd changes */
void launch_scene_draw(const game_t *g, float t, float dt); /* translucent list must be open */
