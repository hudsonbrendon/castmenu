#include "launch.h"
#include "gdemu.h"
#include "settings.h"
#include "loader/loader.h"
#include <kos.h>
#include <string.h>

void launch_exit_to_bios(void) {
    arch_menu();
    __builtin_unreachable();
}

void launch_game(const game_t *g) {
    settings_t *s = settings_get();
    s->last_slot = g->slot; settings_push_recent(g->slot); settings_mark_launch(g->slot, (uint32_t)rtc_unix_secs()); settings_save();
    loader_params_t p = {
        .region_free = s->region_free, .force_vga = s->force_vga, .igr = s->igr,
        .boot_intro = s->boot_intro, .sega_license = s->sega_license,
        .game_region = g->region[0] == 'J' ? 0 : g->region[0] == 'U' ? 1 : 2,
        .disc_type = g->is_gd, .menu_slot = 1,
    };
    gdemu_img_set(g->slot);
    thd_sleep(250);
    loader_run(&p);
}
