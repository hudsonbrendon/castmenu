#include "ui/video.h"
#include "launch.h"
#include "gdemu.h"
#include "settings.h"
#include "loader/loader.h"
#include <kos.h>
#include <string.h>
#include <arch/exec.h>
#include <arch/cache.h>

/* GDMENU's own loader, byte-identical sequence to GDMENU's launch (docs/REVERSE_ENGINEERING.md §4):
 * image read to 0x8CD00000, params at 0xACCFFF00, arch_exec() copies it to 0x8C010000 and jumps. */
static int launch_with_gdmenu_loader(const loader_params_t *p) {
    file_t f = fs_open("/cd/loader.bin", O_RDONLY); if (f < 0) return -1;
    size_t len = fs_total(f);
    uint8_t *img = (uint8_t *)0x8CD00000;
    if (fs_read(f, img, len) != (ssize_t)len) { fs_close(f); return -1; }
    fs_close(f);
    memcpy((void *)0xACCFFF00, p, sizeof *p);
    dcache_flush_range(0x8CCFFF00, 64);
    arch_exec(img, (len + 3) & ~3u);
    return -1;
}
int launch_gdmenu_loader_present(void) { file_t f = fs_open("/cd/loader.bin", O_RDONLY); if (f < 0) return 0; fs_close(f); return 1; }

void launch_exit_to_bios(void) {
    arch_menu();
    __builtin_unreachable();
}

void launch_game(const game_t *g) {
    video_stop();
    settings_t *s = settings_get();
    s->last_slot = g->slot; settings_push_recent(g->slot); settings_mark_launch(g->slot, (uint32_t)rtc_unix_secs()); settings_save();
    loader_params_t p = {
        .region_free = s->region_free, .force_vga = s->force_vga, .igr = s->igr,
        .boot_intro = s->boot_intro, .sega_license = s->sega_license,
        .game_region = g->region[0] == 'J' ? 0 : g->region[0] == 'U' ? 1 : 2,
        .disc_type = g->is_gd, .menu_slot = 1,
    };
    gdemu_img_set(g->slot);
    thd_sleep(600);   /* give GDEMU time to mount the new image before the BIOS re-reads the TOC */
    if (s->loader_kind == 1) launch_with_gdmenu_loader(&p);   /* falls through to the open loader if missing */
    loader_run(&p);
}
