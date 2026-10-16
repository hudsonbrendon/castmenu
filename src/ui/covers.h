/* Small LRU of cover textures: /cd/covers/NNN.tex (card tool) or .pvr. */
#pragma once
#include "draw.h"
#include <stdint.h>
void covers_init(void);
const texture_t *cover_get(uint16_t slot);
const texture_t *shot_get(uint16_t slot, int n);   /* /cd/shots/NNN_n.tex (1 = gameplay, 2 = title), NULL if none */   /* NULL if none; loads synchronously */
void covers_flush(void);
