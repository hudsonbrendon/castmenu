/* Small LRU of cover textures: /cd/covers/NNN.tex (card tool) or .pvr. */
#pragma once
#include "draw.h"
#include <stdint.h>
void covers_init(void);
const texture_t *cover_get(uint16_t slot);   /* NULL if none; loads synchronously */
void covers_flush(void);
