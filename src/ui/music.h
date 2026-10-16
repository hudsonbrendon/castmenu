/* Music: the selected game's theme (music.idx, AICA ADPCM streamed from the GD-ROM) and menu
 * background tracks (slots >= 1000 in the same index). */
#pragma once
#include <stdint.h>
void music_init(void);                        /* reads /cd/music.idx, sets up the stream */
int  music_has(uint16_t slot);
void music_update(uint16_t slot, float dt);   /* slot under the cursor (0 = none); call every frame */
void music_stop(void);                        /* before launching: no more disc reads */
