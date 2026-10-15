/* Texture loaders.
 *  .tex  : Cast Menu raw format written by tools/cast_card/tex.py
 *          u32 'NTEX', u16 w, u16 h, u16 iw, u16 ih, u32 fmt(PVR flags), data
 *  .pvr  : standard Sega PVR (optional GBIX header), twiddled/VQ/rect, 16bpp.
 *          Used for 0GDTEX.PVR covers lifted from game discs. */
#include "draw.h"
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int upload(texture_t *t, const void *data, size_t len) {
    t->ptr = pvr_mem_malloc(len);
    if (!t->ptr) return -1;
    pvr_txr_load(data, t->ptr, len);
    return 0;
}

static int load_file(const char *path, uint8_t **out, size_t *len) {
    file_t f = fs_open(path, O_RDONLY); if (f < 0) return -1;
    *len = fs_total(f); *out = memalign(32, *len + 32);
    if (!*out) { fs_close(f); return -1; }
    ssize_t r = fs_read(f, *out, *len); fs_close(f);
    if (r != (ssize_t)*len) { free(*out); return -1; }
    return 0;
}

static int load_ntex(const uint8_t *d, size_t len, texture_t *t) {
    if (len < 16 || memcmp(d, "NTEX", 4)) return -1;
    t->w = d[4] | d[5] << 8; t->h = d[6] | d[7] << 8; t->iw = d[8] | d[9] << 8; t->ih = d[10] | d[11] << 8;
    t->fmt = d[12] | d[13] << 8 | d[14] << 16 | (uint32_t)d[15] << 24;
    return upload(t, d + 16, len - 16);
}

static int load_pvr(const uint8_t *d, size_t len, texture_t *t) {
    size_t off = 0;
    if (len >= 16 && !memcmp(d, "GBIX", 4)) off = 8 + (d[4] | d[5] << 8 | d[6] << 16 | d[7] << 24);
    if (off + 16 > len || memcmp(d + off, "PVRT", 4)) return -1;
    uint8_t px = d[off + 8], ty = d[off + 9];
    t->w = t->iw = d[off + 12] | d[off + 13] << 8; t->h = t->ih = d[off + 14] | d[off + 15] << 8;
    uint32_t fmt;
    switch (px) { case 0: fmt = PVR_TXRFMT_ARGB1555; break; case 1: fmt = PVR_TXRFMT_RGB565; break; case 2: fmt = PVR_TXRFMT_ARGB4444; break; default: return -1; }
    switch (ty) {
        case 0x01: case 0x02: case 0x0D: fmt |= PVR_TXRFMT_TWIDDLED; break;      /* twiddled / twiddled+mips / tw rect */
        case 0x03: case 0x04: case 0x10: fmt |= PVR_TXRFMT_TWIDDLED | PVR_TXRFMT_VQ_ENABLE; break; /* VQ / VQ+mips / small VQ */
        case 0x09: case 0x0B: fmt |= PVR_TXRFMT_NONTWIDDLED; break;               /* rectangle / stride */
        default: return -1;
    }
    t->fmt = fmt;
    const uint8_t *data = d + off + 16; size_t dlen = len - off - 16;
    if (ty == 0x02 || ty == 0x04) { /* skip mipmaps: hardware wants the largest level last; just take last w*h block */
        size_t lvl = (size_t)t->w * t->h * (fmt & PVR_TXRFMT_VQ_ENABLE ? 1 : 2) / (fmt & PVR_TXRFMT_VQ_ENABLE ? 4 : 1);
        if (fmt & PVR_TXRFMT_VQ_ENABLE) { data += 2048; dlen -= 2048; }
        if (dlen > lvl) { data += dlen - lvl; dlen = lvl; }
        if (fmt & PVR_TXRFMT_VQ_ENABLE) { /* re-prepend codebook */
            uint8_t *tmp = memalign(32, 2048 + lvl); if (!tmp) return -1;
            memcpy(tmp, d + off + 16, 2048); memcpy(tmp + 2048, data, lvl);
            int r = upload(t, tmp, 2048 + lvl); free(tmp); return r;
        }
    }
    return upload(t, data, dlen);
}

int texture_load(const char *path, texture_t *t) {
    uint8_t *d; size_t len; memset(t, 0, sizeof *t);
    if (load_file(path, &d, &len)) return -1;
    int r = !memcmp(d, "NTEX", 4) ? load_ntex(d, len, t) : load_pvr(d, len, t);
    free(d); return r;
}

void texture_free(texture_t *t) { if (t->ptr) pvr_mem_free(t->ptr); t->ptr = 0; }

int texture_solid(texture_t *t) {
    static uint16_t px[8 * 8] __attribute__((aligned(32)));
    for (int i = 0; i < 64; i++) px[i] = 0xFFFF;
    t->w = t->h = t->iw = t->ih = 8; t->fmt = PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED;
    return upload(t, px, sizeof px);
}

int texture_from_alpha(texture_t *t, const uint8_t *alpha, int w, int h) {
    uint16_t *px = memalign(32, (size_t)w * h * 2); if (!px) return -1;
    for (int i = 0; i < w * h; i++) px[i] = (uint16_t)((alpha[i] >> 4) << 12 | 0x0FFF);
    t->w = t->iw = w; t->h = t->ih = h; t->fmt = PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED;
    int r = upload(t, px, (size_t)w * h * 2); free(px); return r;
}
