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

#define SHOT_N 6
static struct { uint16_t slot; uint8_t n, missing; uint32_t tick; texture_t t; } sc[SHOT_N];
const texture_t *shot_get(uint16_t slot, int n) {
    if (!slot) return NULL;
    tick++;
    int lru = 0;
    for (int i = 0; i < SHOT_N; i++) {
        if (sc[i].slot == slot && sc[i].n == n) { sc[i].tick = tick; return sc[i].missing ? NULL : &sc[i].t; }
        if (sc[i].tick < sc[lru].tick) lru = i;
    }
    texture_free(&sc[lru].t);
    sc[lru].slot = slot; sc[lru].n = n; sc[lru].tick = tick; sc[lru].missing = 0;
    char path[64]; snprintf(path, sizeof path, "/cd/shots/%03d_%d.tex", slot, n);
    if (texture_load(path, &sc[lru].t)) { sc[lru].missing = 1; return NULL; }
    return &sc[lru].t;
}
