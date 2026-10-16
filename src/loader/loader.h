/* Open bootloader interface. Implemented in loader.c (resident code copied to
 * high RAM) — behaviour documented in docs/REVERSE_ENGINEERING.md. */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t region_free, force_vga, igr, boot_intro, sega_license;
    uint32_t game_region;   /* 0 J, 1 U, 2 E */
    uint32_t disc_type;     /* 0 CD, 1 GD */
    uint32_t menu_slot;     /* GDEMU slot to return to on IGR */
    uint32_t n_codes;       /* cheat (code, value) pairs the menu left at LDR_CODES */
} loader_params_t;
#define LDR_CODES 0x8CCFF800u   /* 2 KB below the parameter block: up to 78 pairs */
void loader_run(const loader_params_t *p) __attribute__((noreturn));
