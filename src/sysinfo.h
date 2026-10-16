#pragma once
#include "gdemu.h"
typedef struct {
    gdemu_version_t gdemu;
    char bios_version[16];   /* e.g. "1.01d" */
    char region[16];         /* JAPAN / USA / EUROPE */
    char broadcast[8];       /* NTSC / PAL */
    char cable[16];          /* VGA / RGB / COMPOSITE */
    char lang[8];
    int  region_code;        /* 0 J, 1 U, 2 E */
    int  cable_code;         /* 0 VGA, 2 RGB, 3 COMPOSITE */
    char maple[4][48];       /* per port A-D: product name + sub-devices, "" when empty */
} sysinfo_t;
void sysinfo_maple(sysinfo_t *out);   /* cheap, call on entering the screen (devices hot-plug) */
void sysinfo_collect(sysinfo_t *out);
