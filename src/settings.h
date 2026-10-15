/* Persistent settings, saved to VMU as CASTMENU.SYS (and mirrored in RAM). */
#pragma once
#include <stdint.h>
#include "list.h"

#define SETTINGS_MAGIC   0x4E424C41 /* 'NBLA' */
#define SETTINGS_VERSION 2
#define NICK_LEN 16
#define AVATAR_LEN 16
#define SERVER_LEN 40
#define FAV_WORDS        32         /* 1024 bits >= 999 slots */

typedef struct {
    uint32_t magic, version;
    uint8_t region_free, force_vga, igr, boot_intro, sega_license;
    uint8_t theme;            /* index into theme list */
    uint8_t view;             /* 0 coverflow, 1 list, 2 grid */
    uint8_t sort;             /* sort_mode_t */
    uint8_t lang;             /* 0 pt, 1 en, 2 es, 3 de, 4 fr, 5 it */
    uint8_t screensaver_min;  /* 0 = off */
    uint8_t hide_multidisc;
    uint8_t cover_mode;       /* 0 off, 1 on */
    uint16_t last_slot;       /* resume cursor on the last launched game */
    uint16_t scan_delay_ms;   /* slow-scan wait after image switch */
    uint32_t favorites[FAV_WORDS];
    uint16_t recent[8];
    uint32_t launches;
    /* v2 */
    char nick[NICK_LEN];
    char avatar[AVATAR_LEN];  /* file name under /cd/avatars without extension */
    char server[SERVER_LEN];  /* host:port of the Cast Chat server */
    uint8_t net_auto;         /* connect at boot */
    uint8_t server_user;      /* server was typed by the user: CASTMENU.INI must not override it */
    uint8_t loader_kind;      /* 0 Cast open loader, 1 GDMENU's loader.bin from /cd */
    uint8_t pad[1];
    uint32_t last_launch_ts;  /* RTC seconds when the last game was launched (play-time tracking) */
    uint16_t last_launch_slot;
    uint16_t playtime[1000];  /* minutes per slot */
} settings_t;

settings_t *settings_get(void);
void settings_init(void);           /* load from VMU or defaults */
int  settings_save(void);           /* 0 ok */
void settings_apply_favorites(gamelist_t *l);
void settings_set_favorite(uint16_t slot, int on);
void settings_push_recent(uint16_t slot);
void settings_mark_launch(uint16_t slot, uint32_t now);   /* call right before jumping to the game */
void settings_account_playtime(uint32_t now);            /* call at boot: credits time since last launch */
