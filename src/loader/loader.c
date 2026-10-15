/* KOS-side bridge to the open loader (ldr/): publish the parameter block and
 * hand the raw image to KOS's arch_exec_at(), exactly like GDMENU does with
 * arch_exec(). That call is what shuts the kernel down properly (PVR, Maple
 * DMA, timers, IRQs, SR/VBR/FPSCR) before the loader re-enters the BIOS;
 * jumping straight into the image leaves KOS interrupt sources armed and the
 * BIOS boot hangs on a black screen once it re-enables interrupts. */
#include "loader.h"
#include <kos.h>
#include <arch/cache.h>
#include <arch/exec.h>
#include <string.h>

#define LDR_IMAGE   "/rd/cast_ldr.bin"
#define LDR_BASE    0x8CE00000u     /* link address of ldr/ (top 2 MB, same as GDMENU) */
#define LDR_PARAMS  0x8CCFFF00u

void loader_run(const loader_params_t *p) {
    file_t f = fs_open(LDR_IMAGE, O_RDONLY);
    if (f < 0) { arch_menu(); __builtin_unreachable(); }
    size_t len = fs_total(f);
    uint8_t *img = memalign(32, (len + 31) & ~31u);
    if (!img || fs_read(f, img, len) != (ssize_t)len) { arch_menu(); __builtin_unreachable(); }
    fs_close(f);
    memcpy((void *)(LDR_PARAMS | 0x20000000), p, sizeof *p);   /* P2: straight to RAM */
    dcache_flush_range(LDR_PARAMS, 64);
    arch_exec_at(img, (len + 3) & ~3u, LDR_BASE);                /* never returns */
    __builtin_unreachable();
}
