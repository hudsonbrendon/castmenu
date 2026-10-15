#include "sysinfo.h"
#include <kos.h>
#include <dc/flashrom.h>
#include <dc/video.h>
#include <string.h>
#include <stdio.h>

/* The boot ROM header mirrors IP.BIN's layout: the version string sits at
 * offset 0x4A of the ROM image (uncached P2 window at 0xA0000000). Custom
 * ROMs (japanese_cake 1.032) keep the same slot. */
static void bios_version(char *out, int cap) {
    const volatile unsigned char *rom = (const volatile unsigned char *)0xA0000000;
    int n = 0;
    for (int i = 0x4A; i < 0x50 && n < cap - 1; i++) { unsigned char c = rom[i]; if (c < 32 || c > 126) break; out[n++] = c; }
    out[n] = 0;
    if (!n) strncpy(out, "?", cap);
}

void sysinfo_collect(sysinfo_t *s) {
    memset(s, 0, sizeof *s);
    gdemu_get_version(&s->gdemu);
    bios_version(s->bios_version, sizeof s->bios_version);
    int r = flashrom_get_region();
    s->region_code = r == FLASHROM_REGION_JAPAN ? 0 : r == FLASHROM_REGION_US ? 1 : 2;
    strcpy(s->region, r == FLASHROM_REGION_JAPAN ? "JAPAO" : r == FLASHROM_REGION_US ? "EUA/CANADA" : r == FLASHROM_REGION_EUROPE ? "EUROPA" : "?");
    int c = vid_check_cable();
    s->cable_code = c;
    strcpy(s->cable, c == CT_VGA ? "VGA" : c == CT_RGB ? "RGB/SCART" : c == CT_COMPOSITE ? "COMPOSITO/RF" : "?");
    flashrom_syscfg_t cfg;
    if (flashrom_get_syscfg(&cfg) == 0) {
        strcpy(s->lang, cfg.language == FLASHROM_LANG_JAPANESE ? "JP" : cfg.language == FLASHROM_LANG_ENGLISH ? "EN" : cfg.language == FLASHROM_LANG_GERMAN ? "DE" : cfg.language == FLASHROM_LANG_FRENCH ? "FR" : cfg.language == FLASHROM_LANG_SPANISH ? "ES" : cfg.language == FLASHROM_LANG_ITALIAN ? "IT" : "?");
    }
    strcpy(s->broadcast, r == FLASHROM_REGION_EUROPE ? "PAL" : "NTSC");
}
