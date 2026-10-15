#include "ipbin.h"
#include <string.h>

static void field(char *dst, const unsigned char *src, int n) {
    memcpy(dst, src, n); dst[n] = 0;
    while (n > 0 && dst[n - 1] == ' ') dst[--n] = 0;
}

int ipbin_parse(const unsigned char *h, ipbin_t *o) {
    memset(o, 0, sizeof *o);
    if (memcmp(h, "SEGA SEGAKATANA ", 16)) return -1;
    field(o->hw, h + 0x00, 16);  field(o->maker, h + 0x10, 16);
    field(o->crc, h + 0x20, 5);  field(o->device, h + 0x25, 6);
    field(o->area, h + 0x30, 8); field(o->periph, h + 0x38, 8);
    field(o->product, h + 0x40, 10); field(o->version, h + 0x4A, 6);
    { char *w = o->product; for (const char *r = o->product; *r; r++) if (*r != '-' && *r != ' ') *w++ = *r; *w = 0; } /* match card tool: "HDR-0016" -> "HDR0016" */
    field(o->date, h + 0x50, 8);  field(o->boot, h + 0x60, 16);
    field(o->company, h + 0x70, 16); field(o->name, h + 0x80, 64);
    o->is_gd = memcmp(h + 0x25, "GD-ROM", 6) == 0;
    o->disc_num = h[0x2B]; o->disc_total = h[0x2D];
    int r = 0;
    if (h[0x30] == 'J') o->region[r++] = 'J';
    if (h[0x31] == 'U') o->region[r++] = 'U';
    if (h[0x32] == 'E') o->region[r++] = 'E';
    /* periph: hex string, bit 4 (0x10) of the first byte = VGA box support */
    unsigned long p = 0;
    for (int i = 0; i < 8; i++) { /* 7 hex digits, space padded */
        char c = h[0x38 + i];
        int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) break;
        p = p << 4 | v;
    }
    o->vga = (p >> 4) & 1;
    return 0;
}
