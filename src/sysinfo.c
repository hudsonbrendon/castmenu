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

/* Maple devices as the console sees them. Bluetooth adapters (BlueRetro) show up as
 * the controller/VMU/jump pack they emulate: the bus carries no vendor status. */
void sysinfo_maple(sysinfo_t *s) {
    for (int p = 0; p < 4; p++) {
        char *o = s->maple[p]; o[0] = 0;
        maple_device_t *d = maple_enum_dev(p, 0);
        if (!d || !d->valid) continue;
        int n = 0; while (n < 29 && d->info.product_name[n] && d->info.product_name[n] != ' ' + 0x80) n++;
        while (n > 0 && d->info.product_name[n - 1] == ' ') n--;
        snprintf(o, 48, "%.*s", n, d->info.product_name);
        uint32_t sub = 0;
        for (int u = 1; u < 6; u++) { maple_device_t *e = maple_enum_dev(p, u); if (e && e->valid) sub |= e->info.functions; }
        if (sub & MAPLE_FUNC_MEMCARD) strncat(o, " +VMU", 47 - strlen(o));
        if (sub & MAPLE_FUNC_PURUPURU) strncat(o, " +Vib", 47 - strlen(o));
        if (sub & MAPLE_FUNC_MICROPHONE) strncat(o, " +Mic", 47 - strlen(o));
    }
}
