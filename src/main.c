/* Cast Menu — open menu for GDEMU (Sega Dreamcast). MIT. */
#include <kos.h>
#include "settings.h"
#include "list.h"
#include "ui/ui.h"
#include "launch.h"

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_FS_ROMDISK | INIT_NET);  /* romdisk.o is linked via KOS_ROMDISK_DIR */

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    settings_init();
    settings_account_playtime((uint32_t)rtc_unix_secs());
    ui_init();
    list_load(ui_boot_progress);
    while (!ui_wants_exit()) ui_frame();
    launch_exit_to_bios();
}
