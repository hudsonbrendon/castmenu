/* Bitmap font: <base>.tex atlas + <base>.fnt metrics (tools/cast_card/mkfont.py).
 * .fnt: 'NFNT', u8 line_h, u8 base, u16 pad, then 224 x {u16 x, u16 y, u8 w, u8 h, s8 xoff, s8 yoff, u8 adv, u8 pad} */
#include "draw.h"
#include <kos.h>
#include <stdio.h>
#include <string.h>

int font_load(const char *base, font_t *f) {
    char path[128]; memset(f, 0, sizeof *f);
    snprintf(path, sizeof path, "%s.tex", base);
    if (texture_load(path, &f->tex)) return -1;
    snprintf(path, sizeof path, "%s.fnt", base);
    file_t fd = fs_open(path, O_RDONLY); if (fd < 0) return -1;
    uint8_t hdr[8]; if (fs_read(fd, hdr, 8) != 8 || memcmp(hdr, "NFNT", 4)) { fs_close(fd); return -1; }
    f->line_h = hdr[4]; f->base = hdr[5];
    uint8_t rec[10];
    for (int i = 0; i < 224; i++) {
        if (fs_read(fd, rec, 10) != 10) break;
        f->g[i].x = rec[0] | rec[1] << 8; f->g[i].y = rec[2] | rec[3] << 8;
        f->g[i].w = rec[4]; f->g[i].h = rec[5]; f->g[i].xoff = (int8_t)rec[6]; f->g[i].yoff = (int8_t)rec[7]; f->g[i].adv = rec[8];
    }
    fs_close(fd);
    return 0;
}
