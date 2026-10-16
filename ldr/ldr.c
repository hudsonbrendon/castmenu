/* Cast Menu open bootloader for GDEMU — clean-room C reimplementation of the
 * behaviour observed in GDMENU 0.6's loader.bin (see docs/REVERSE_ENGINEERING.md,
 * section "loader.bin"). Runs with no OS: the menu copies this image to
 * 0x8CE00000, writes loader_params_t at 0x8CCFFF00, and jumps to _start.
 *
 * What it does (in order), each step traced to the decompiled original:
 *   1. blank the screen + re-init maple DMA (so the BIOS starts clean)
 *   2. clear a stale KOS region if a magic marker is present
 *   3. restore the BIOS's RAM-resident image from ROM (as a cold boot would)
 *   4. patch that RAM image: region check, VGA check, boot animation,
 *      SEGA license screen, in-game reset hook
 *   5. jump into the BIOS boot path at 0x8C00B500 with r1 = media flag
 *
 * Nothing here is derived from SEGA source; the patch bytes are the opcode
 * sequences GDMENU writes, reproduced verbatim as data. */
#include <stdint.h>

typedef struct {
    uint32_t region_free, force_vga, igr, boot_intro, sega_license;
    uint32_t game_region, disc_type, need_game_fix;
    uint32_t n_codes;           /* cheat codes at CODES_SRC: (code, value) pairs */
} loader_params_t;

/* Cheats: the menu leaves (code, value) pairs at CODES_SRC; we park them next to GDMENU's
 * region cave (0x8C003CC0..0x8C003D02) and hook the GD-ROM syscall vector with the stub from
 * start.S. Only constant-write codes (types 0/1/2) are honoured; see docs. */
#define CODES_SRC   0xACCFF800u
#define CHEAT_STUB  0xAC003D10u
#define CHEAT_ORIG  0xAC003D7Cu
#define CHEAT_TABLE 0xAC003D80u
#define CHEAT_MAX   78                     /* table must end before 0x8C004000 (GDMENU's clear region) */
#define GD_VECTOR   0xAC0000BCu
extern const uint8_t cheat_hook_start[], cheat_hook_end[];   /* start.S (C names get the leading underscore) */

#define PARAMS ((volatile loader_params_t *)0xACCFFF00)
#define ROM(off)  ((const volatile uint8_t *)(0xA0000000 + (off)))
#define W16(a, v) (*(volatile uint16_t *)(a) = (uint16_t)(v))
#define W32(a, v) (*(volatile uint32_t *)(a) = (uint32_t)(v))
#define R8(a)     (*(volatile uint8_t *)(a))
#define R32(a)    (*(volatile uint32_t *)(a))

/* ---- helpers (no libc) ---- */
static void copy32(uint32_t *dst, const uint32_t *src, uint32_t words) { while (words--) *dst++ = *src++; }
static void fill16(uint16_t *dst, uint16_t v, uint32_t n) { while (n--) *dst++ = v; }

/* ---- bootROM identification: version string lives at ROM+0x7CB ("1.004" / "1.01d" / "1.032") ---- */
static int bootrom_is_custom(void) { return R8(0xA00007CF) >= '3' && R8(0xA00007D0) >= '2'; }  /* japanese_cake >= 1.032 */
static int bootrom_is_1_01d(void)  { return R8(0xA00007CF) == '1' && R8(0xA00007D0) == 'd'; }

/* ---- step 1: video + maple ---- */
static uint32_t cable_type(void) {
    volatile uint32_t *pctra = (volatile uint32_t *)0xFF80002C;
    *pctra = (*pctra & 0xFFF0FFFF) | 0x000A0000;
    return (*(volatile uint16_t *)0xFF800030 >> 8) & 3;   /* 0 VGA, 2 RGB, 3 composite */
}
static void video_blank(void) {
    volatile uint32_t *pvr = (volatile uint32_t *)0xA05F8000;
    uint32_t vga = cable_type() == 0;
    pvr[0x44 / 4] &= ~1u;                         /* FB_R_CTRL: display off */
    pvr[0x40 / 4] = 0;                             /* border black */
    fill16((uint16_t *)0xA5000000, 0, 640 * 480);  /* clear framebuffer */
    pvr[0x50 / 4] = 0; pvr[0x54 / 4] = 640 * 2;    /* FB_R_SOF1/2 */
    pvr[0x5C / 4] = (((480 - 1) << 10) | (640 * 2 / 4 - 1)) | (vga ? 0x00100000 : 0);
    pvr[0x44 / 4] = (vga ? 0x00800000 : 0) | 4 | 1; /* RGB565, display on */
    (void)vga;
}
static void maple_init(void) {
    W32(0xA05F6C8C, 0x6155404F);  /* MDAPRO: unlock */
    W32(0xA05F6C10, 0);           /* MDTSEL: start on vblank off */
    W32(0xA05F6C80, 0xC3500000);  /* MSYS: timeout/speed */
    W32(0xA05F6C14, 1);           /* MDEN */
}

/* ---- step 2: GDMENU clears 0x8C004000..0x8C248000 if a 0xDEADBEEF marker sits at 0x8C004004 ---- */
static void clear_marked_region(void) {
    if (R32(0x8C004004) == 0xDEADBEEF) { uint32_t *p = (uint32_t *)0x8C004000; for (uint32_t i = 0; i <= 0x90FFF; i++) *p++ = 0; }
}

/* ---- step 3: restore BIOS RAM image from ROM ---- */
static void restore_bios_ram(void) {
    if (bootrom_is_custom()) {
        copy32((uint32_t *)0x8C000100, (const uint32_t *)0xA0005100, 0x000900C0);
        copy32((uint32_t *)0x8C000400, (const uint32_t *)0xA0000400, 0x0007FF00);
    } else {
        copy32((uint32_t *)0x8C000100, (const uint32_t *)0xA0000100, 0x40);
        copy32((uint32_t *)0x8C008000, (const uint32_t *)0xA0008000, 0x0007E000);
    }
}

/* ---- custom bootROM path: the 1.032 ROM exposes a syscall (vector 0x8C0000B0, r7=1)
 *      that takes the patch flags directly ---- */
static void custom_bootrom_set_flags(uint32_t flags) {
    void (*sys)(uint32_t, uint32_t, uint32_t, uint32_t) = (void (*)(uint32_t, uint32_t, uint32_t, uint32_t))R32(0x8C0000B0);
    sys(flags, 0, 1, 1);
}

/* ---- step 4: patch the RAM-resident BIOS (stock ROMs 1.004 / 1.01d) ---- */
static const uint16_t region_cave[] = { /* written at 0x8C003CC0; two slots get the region digit */
    0xE201, 0x3720, 0xD30A, 0x8B10, 0x022A, 0xD10A, 0x430B, 0x2122, 0xD30A,
    /*0x3CD2*/ 0xE230, 0x2320, 0x73FE, /*0x3CD8*/ 0xE230, 0x2320, 0x73EE, 0x03B3, 0xD304, 0x6232, 0x422A,
    0x000B, 0x0009, 0x432B, 0x0009, 0x0009, 0x3D00, 0x8C00, 0x3CF8, 0x8C00, 0x0000, 0x0000, 0x0074, 0x8C00 };

static void patch_bios(const loader_params_t *p) {
    int d = bootrom_is_1_01d();
    if (p->igr) W16(0xAC000198, 0xFF86);

    if (p->force_vga) {              /* neutralise the AV-cable/VGA-flag check: nop + mov #1,r0 */
        if (d) { W16(0x8C00054C, 0x0009); W16(0x8C00054E, 0xE001); }
        else   { W16(0x8C000536, 0x0009); W16(0x8C000538, 0xE001); }
    }
    if (p->region_free) {            /* region check -> code cave that makes the console match the game */
        uint16_t *cave = (uint16_t *)0x8C003CC0;
        for (unsigned i = 0; i < sizeof region_cave / 2; i++) cave[i] = region_cave[i];
        W16(0x8C003CD2, 0xE230 + (p->game_region >> 1));
        W16(0x8C003CD8, 0xE230 + p->game_region);
        W16(0x8C0004FA, 0xD103); W16(0x8C0004FC, 0xD203);
        W16(0x8C0004FE, p->need_game_fix ? 0x0009 : 0x2212);
        W16(0x8C000500, 0xE100);
        W16(0x8C000502, d ? 0xA016 : 0xA00C);
        W16(0x8C000504, 0x6213); W16(0x8C000506, 0x0009);
        W16(0x8C000508, 0x3CC0); W16(0x8C00050A, 0x8C00);   /* -> 0x8C003CC0 */
        W16(0x8C00050C, 0x00B8); W16(0x8C00050E, 0x8C00);   /* -> 0x8C0000B8 */
    }
    if (!p->boot_intro && p->disc_type == 1) {   /* skip boot animation (GD-ROM only) */
        W16(0x8C00CAFA, 0xE501); W16(0x8C00CB0C, 0xE500);
    }
    if (!p->sega_license && p->disc_type == 1) { /* skip SEGA license screen (GD-ROM only) */
        W16(0x8C000424, 0xD073); W16(0x8C00044C, 0xD46A); W16(0x8C00044E, 0xE307); W16(0x8C000464, 0xD5D1);
        if (!p->region_free) { W16(0x8C0004FA, d ? 0xA010 : 0xA01A); W16(0x8C0004FC, 0x0009); }
        if (d) W16(0x8C000592, 0xC784); else W16(0x8C000590, 0xC784);
        static const uint16_t seq[] = { 0x9319, 0x14BC, 0x0009, 0x024C, 0xA021, 0x0E38, 0xE200, 0x143A, 0xE200, 0x0242, 0xE500, 0x01A0, 0xA03D };
        W16(0x8C0005BA, seq[0]);
        for (unsigned i = 1; i < sizeof seq / 2; i++) W16(0x8C0005D0 + (i - 1) * 2, seq[i]);
        W16(0x8C0005F0, 0x0100); W16(0x8C0005F2, 0x0000);
        W32(0x8C0005F4, d ? 0x8C000AD4 : 0x8C000AE4);
        W32(0x8C0005F8, 0xAC008300);
        W16(0x8C0007A4, 0x4301); W16(0x8C0007A6, 0x3044); W16(0x8C0007A8, 0x3130); /* "\1CD001" */
        W32(0x8C0007AC, 0x8CE00000);                                               /* back-reference to this loader */
        W16(0x8C00086E, 0xE000);
    }
}

/* ---- cheats: copy stub + table through P2 (uncached, no stale cache lines), hook the vector ---- */
static void install_cheats(uint32_t n) {
    if (n > CHEAT_MAX) n = CHEAT_MAX;
    const uint8_t *s = cheat_hook_start; uint8_t *d = (uint8_t *)CHEAT_STUB;
    for (uint32_t i = 0; i < (uint32_t)(cheat_hook_end - cheat_hook_start); i++) d[i] = s[i];
    volatile uint32_t *src = (volatile uint32_t *)CODES_SRC, *tab = (volatile uint32_t *)CHEAT_TABLE;
    for (uint32_t i = 0; i < n * 2; i++) tab[i] = src[i];
    tab[n * 2] = 0;
    W32(CHEAT_ORIG, R32(GD_VECTOR));
    W32(GD_VECTOR, 0x8C003D10);
}

/* ---- step 5 ---- */
extern void bios_enter(uint32_t media_flag) __attribute__((noreturn));

void loader_main(void) {
    loader_params_t p = *PARAMS;
    video_blank();
    maple_init();
    clear_marked_region();
    restore_bios_ram();
    if (bootrom_is_custom()) {
        uint32_t f = 0x30;
        if (p.disc_type) { if (!p.boot_intro) f = 0x31; if (!p.sega_license) f |= 2; }
        custom_bootrom_set_flags(f);
    } else {
        patch_bios(&p);
    }
    if (p.n_codes) install_cheats(p.n_codes);
    bios_enter((R8(0x8C000072) & 7) == 2 ? 0 : 1);
}
