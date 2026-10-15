/* KOS-side bridge to the open loader (ldr/): copy the raw image to its link
 * address, publish the parameter block, shut KOS down and jump. */
#include "loader.h"
#include <kos.h>
#include <arch/cache.h>
#include <arch/irq.h>
#include <string.h>

#define LDR_IMAGE   "/rd/cast_ldr.bin"
#define LDR_BASE    0x8CE00000u
#define LDR_BASE_P2 0xACE00000u
#define LDR_PARAMS  0x8CCFFF00u

void loader_run(const loader_params_t *p) {
    file_t f = fs_open(LDR_IMAGE, O_RDONLY);
    if (f < 0) { arch_menu(); __builtin_unreachable(); }
    size_t len = fs_total(f);
    uint8_t *img = malloc(len + 32);
    if (!img || fs_read(f, img, len) != (ssize_t)len) { arch_menu(); __builtin_unreachable(); }
    fs_close(f);

    irq_disable();  /* KOS: masks all interrupts */
    /* params first (the loader reads them through the P2 alias) */
    memcpy((void *)(LDR_PARAMS | 0x20000000), p, sizeof *p);
    memcpy((void *)LDR_BASE_P2, img, len);
    dcache_flush_range(LDR_BASE, len + 64);
    icache_flush_range(LDR_BASE, len + 64);
    dcache_flush_range(LDR_PARAMS, 64);
    /* jump through P2 so the loader runs uncached like GDMENU's does */
    void (*entry)(void) = (void (*)(void))LDR_BASE_P2;
    entry();
    __builtin_unreachable();
}
