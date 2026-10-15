/* Open bootloader interface. Implemented in loader.c (resident code copied to
 * high RAM) — behaviour documented in docs/REVERSE_ENGINEERING.md. */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t region_free, force_vga, igr, boot_intro, sega_license;
    uint32_t game_region;   /* 0 J, 1 U, 2 E */
    uint32_t disc_type;     /* 0 CD, 1 GD */
    uint32_t menu_slot;     /* GDEMU slot to return to on IGR */
} loader_params_t;
void loader_run(const loader_params_t *p) __attribute__((noreturn));
