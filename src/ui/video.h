/* Background video: short clips of the selected game, stored on the high-density
 * area as raw PVR VQ frames (tools/cast_card/cvm.py) and indexed by /cd/videos.idx. */
#pragma once
#include <stdint.h>
void video_init(void);                         /* reads /cd/videos.idx */
int  video_has(uint16_t slot);
void video_update(uint16_t slot, float dt);    /* slot under the cursor (0 = none); handles hover delay + fades */
void video_draw(void);                         /* translucent full-screen layer, call before the theme overlay */
int  video_draw_rect(float x, float y, float w, float h, int alpha); /* the same clip in a window (details); 0 if nothing playing */
void video_stop(void);                         /* before launching a game: no more disc reads */
