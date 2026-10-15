#include "covers.h"
#include <stdio.h>
#include <string.h>

#define CACHE_N 20
static struct { uint16_t slot; uint32_t tick; int missing; texture_t t; } c[CACHE_N];
static uint32_t tick;

void covers_init(void) { memset(c, 0, sizeof c); }
void covers_flush(void) { for (int i = 0; i < CACHE_N; i++) texture_free(&c[i].t); memset(c, 0, sizeof c); }

const texture_t *cover_get(uint16_t slot) {
    if (!slot) return NULL;
    tick++;
    int lru = 0;
    for (int i = 0; i < CACHE_N; i++) {
        if (c[i].slot == slot) { c[i].tick = tick; return c[i].missing ? NULL : &c[i].t; }
        if (c[i].tick < c[lru].tick) lru = i;
    }
    texture_free(&c[lru].t);
    c[lru].slot = slot; c[lru].tick = tick; c[lru].missing = 0;
    char path[64];
    snprintf(path, sizeof path, "/cd/covers/%03d.tex", slot);
    if (texture_load(path, &c[lru].t)) {
        snprintf(path, sizeof path, "/cd/covers/%03d.pvr", slot);
        if (texture_load(path, &c[lru].t)) { c[lru].missing = 1; return NULL; }
    }
    return &c[lru].t;
}
