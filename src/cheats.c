#include "cheats.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hex8(const char *s, uint32_t *v) {
    char *e; *v = (uint32_t)strtoul(s, &e, 16);
    return e - s == 8;
}

int cheats_load(uint16_t slot, cheat_t *out, int max) {
    char path[48]; snprintf(path, sizeof path, "/cd/cheats/%03d.txt", slot);
    FILE *f = fopen(path, "r"); if (!f) return 0;
    int n = 0; cheat_t *c = NULL; char line[96];
    while (fgets(line, sizeof line, f)) {
        char *p = line; while (isspace((unsigned char)*p)) p++;
        char *e = p + strlen(p); while (e > p && isspace((unsigned char)e[-1])) *--e = 0;
        if (!*p) { c = NULL; continue; }                       /* blank line ends a block */
        if (*p == '#' || *p == ';') continue;
        uint32_t a, b;
        if (strlen(p) >= 17 && p[8] == ' ' && hex8(p, &a) && hex8(p + 9, &b)) {
            if (c && c->n < CHEAT_CODES) { c->code[c->n][0] = a; c->code[c->n][1] = b; c->n++; }
            continue;
        }
        if (!c) { if (n >= max) break; c = &out[n++]; memset(c, 0, sizeof *c); strncpy(c->name, p, sizeof c->name - 1); }
    }
    fclose(f);
    int kept = 0;                                              /* drop blocks without codes */
    for (int i = 0; i < n; i++) if (out[i].n) out[kept++] = out[i];
    return kept;
}
int  cheats_enabled(uint16_t slot, int idx) { return slot < 1000 && idx < 8 && (settings_get()->cheats[slot] >> idx) & 1; }
void cheats_toggle(uint16_t slot, int idx)  { if (slot < 1000 && idx < 8) settings_get()->cheats[slot] ^= 1u << idx; }
int cheats_collect(uint16_t slot, uint32_t *pairs, int max_pairs) {
    cheat_t list[CHEAT_MAX]; int n = cheats_load(slot, list, CHEAT_MAX), k = 0;
    for (int i = 0; i < n && i < 8; i++) {
        if (!cheats_enabled(slot, i)) continue;
        for (int j = 0; j < list[i].n && k < max_pairs; j++) { pairs[k * 2] = list[i].code[j][0]; pairs[k * 2 + 1] = list[i].code[j][1]; k++; }
    }
    return k;
}
