#include "settings.h"
#include <string.h>
#include <stdio.h>
#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/vmu_pkg.h>
#endif

static settings_t S;
#define VMU_FILE "CASTMENU.SYS"

settings_t *settings_get(void) { return &S; }

static void defaults(void) {
    memset(&S, 0, sizeof S);
    S.magic = SETTINGS_MAGIC; S.version = SETTINGS_VERSION;
    S.region_free = 1; S.force_vga = 1; S.igr = 1; S.boot_intro = 1; S.sega_license = 1;
    S.hide_multidisc = 1; S.cover_mode = 1; S.scan_delay_ms = 1500; S.screensaver_min = 5;
    strcpy(S.nick, "Player"); strcpy(S.server, "cast.gametracks.local:7350"); S.net_auto = 1;
}

#ifdef _arch_dreamcast
static int vmu_path(char *out, int cap) {
    maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!d) return -1;
    snprintf(out, cap, "/vmu/%c%d/%s", 'a' + d->port, d->unit, VMU_FILE);
    return 0;
}
#endif

void settings_init(void) {
    defaults();
#ifdef _arch_dreamcast
    char path[48]; if (vmu_path(path, sizeof path)) { dbglog(DBG_INFO, "settings: no VMU\n"); return; }
    file_t f = fs_open(path, O_RDONLY); if (f < 0) { dbglog(DBG_INFO, "settings: cannot open %s\n", path); return; }
    size_t len = fs_total(f);
    uint8_t *buf = malloc(len);
    if (buf && fs_read(f, buf, len) == (ssize_t)len) {
        /* The settings block follows the VMU package header + icon. vmu_pkg_parse also checks the CRC,
         * which the file system leaves at 0 on some writes, so locate the block by its magic instead. */
        const uint8_t *data = NULL; size_t avail = 0;
        for (size_t off = 0; off + 8 <= len; off += 4) {
            uint32_t m; memcpy(&m, buf + off, 4);
            if (m == SETTINGS_MAGIC) { data = buf + off; avail = len - off; break; }
        }
        if (data) {
            uint32_t version; memcpy(&version, data + 4, 4);
            size_t need = version == SETTINGS_VERSION ? sizeof S : version == 2 ? SETTINGS_V2_SIZE : 0;   /* v2 -> v3: new fields keep their defaults */
            if (need && avail >= need) { memcpy(&S, data, need); S.version = SETTINGS_VERSION; }
            dbglog(DBG_INFO, "settings: loaded %s (v%lu)\n", path, (unsigned long)version);
        } else dbglog(DBG_INFO, "settings: no Cast Menu block in %s\n", path);
    }
    free(buf); fs_close(f);
#endif
}

int settings_save(void) {
#ifdef _arch_dreamcast
    char path[48]; if (vmu_path(path, sizeof path)) return -1;
    vmu_pkg_t pkg; memset(&pkg, 0, sizeof pkg);
    strcpy(pkg.desc_short, "Cast Menu");
    strcpy(pkg.desc_long, "Cast Menu settings");
    strcpy(pkg.app_id, "CASTMENU");
    pkg.icon_cnt = 1; pkg.icon_anim_speed = 0; pkg.eyecatch_type = VMUPKG_EC_NONE;
    extern uint8_t cast_vmu_icon[512]; extern const uint16_t cast_vmu_pal[16];
    pkg.icon_data = cast_vmu_icon; memcpy(pkg.icon_pal, cast_vmu_pal, sizeof pkg.icon_pal);
    pkg.data_len = sizeof S; pkg.data = (uint8_t *)&S;
    uint8_t *out; int out_len;
    if (vmu_pkg_build(&pkg, &out, &out_len)) return -1;
    fs_unlink(path);
    file_t f = fs_open(path, O_WRONLY); if (f < 0) { free(out); return -1; }
    int ok = fs_write(f, out, out_len) == out_len; fs_close(f); free(out);
    return ok ? 0 : -1;
#else
    return 0;
#endif
}

void settings_apply_favorites(gamelist_t *l) {
    for (int i = 0; i < l->count; i++) {
        uint16_t s = l->items[i].slot;
        l->items[i].favorite = (S.favorites[s / 32] >> (s % 32)) & 1;
    }
}

void settings_set_favorite(uint16_t slot, int on) {
    if (on) S.favorites[slot / 32] |= 1u << (slot % 32); else S.favorites[slot / 32] &= ~(1u << (slot % 32));
}

void settings_mark_launch(uint16_t slot, uint32_t now) { S.last_launch_slot = slot; S.last_launch_ts = now; }
void settings_account_playtime(uint32_t now) {
    if (!S.last_launch_ts || now <= S.last_launch_ts) { S.last_launch_ts = 0; return; }
    uint32_t mins = (now - S.last_launch_ts) / 60;
    if (mins > 12 * 60) mins = 12 * 60;   /* ponytail: console left on overnight counts as 12h */
    if (S.last_launch_slot < 1000) { uint32_t v = S.playtime[S.last_launch_slot] + mins; S.playtime[S.last_launch_slot] = v > 65535 ? 65535 : v; }
    S.last_launch_ts = 0;
}

void settings_push_recent(uint16_t slot) {
    int n = sizeof S.recent / sizeof S.recent[0];
    int at = n - 1;
    for (int i = 0; i < n; i++) if (S.recent[i] == slot) { at = i; break; }
    for (int i = at; i > 0; i--) S.recent[i] = S.recent[i - 1];
    S.recent[0] = slot; S.launches++;
}
