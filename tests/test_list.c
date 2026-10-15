/* Host build: cc -Isrc tests/test_list.c src/list.c src/ini.c src/ipbin.c src/settings.c -o /tmp/t && /tmp/t */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "ini.h"
#include "ipbin.h"
#include "settings.h"

static const char *SAMPLE =
"[GDMENU]\n01.name=CAST MENU\n01.disc=1/1\n01.vga=1\n01.region=JUE\n01.version=V1.000\n01.date=20261015\n\n"
"02.name=SONIC ADVENTURE\n02.disc=1/1\n02.vga=1\n02.region=JUE\n02.version=V1.000\n02.date=19990909\n\n"
"03.name=SHENMUE\n03.disc=1/3\n03.vga=1\n03.region=E\n03.version=V1.000\n03.date=20000120\n\n"
"04.name=SHENMUE\n04.disc=2/3\n04.vga=1\n04.region=E\n04.version=V1.000\n04.date=20000120\n\n"
"05.name=CRAZY TAXI\n05.disc=\n05.vga=0\n05.region=U\n05.version=V1.001\n05.date=20000101\n\n"
"[CASTMENU]\n03.product=HDR0016\n04.product=HDR0016\n05.genre=Racing\n05.cover=1\n";

/* list.c's loader reads /cd/LIST.INI; on the host feed the text through the same callback */
extern int list_ini_cb_for_test(void *u, const char *s, const char *k, const char *v);

int main(void) {
    settings_init();
    memset(&g_list, 0, sizeof g_list);
    char buf[2048]; strcpy(buf, SAMPLE);
    int n = ini_parse_text(buf, list_ini_cb_for_test, NULL);
    assert(n > 20);
    assert(g_list.count == 5);
    g_list.items[0].hidden = 1; /* slot 1 = menu */
    g_list.hide_multidisc_extra = 1; g_list.sort = SORT_NAME;
    list_rebuild_view();
    assert(g_list.view_count == 3);                       /* disc 2/3 hidden, menu hidden */
    assert(!strcmp(list_view_item(0)->name, "CRAZY TAXI"));
    assert(!strcmp(list_view_item(1)->name, "SHENMUE"));
    assert(!strcmp(list_view_item(2)->name, "SONIC ADVENTURE"));
    assert(!strcmp(list_view_item(1)->product, "HDR0016"));
    assert(!strcmp(list_view_item(0)->genre, "Racing") && list_view_item(0)->has_cover && !list_view_item(0)->vga);
    game_t *set[8]; int ns = list_disc_set(list_view_item(1), set, 8);
    assert(ns == 2 && set[0]->disc[0] == '1' && set[1]->disc[0] == '2');
    g_list.hide_multidisc_extra = 0; list_rebuild_view(); assert(g_list.view_count == 4);
    g_list.filter_letter = 'S'; list_rebuild_view(); assert(g_list.view_count == 3);
    g_list.filter_letter = 0; g_list.filter_region = 'U'; list_rebuild_view(); assert(g_list.view_count == 2);
    g_list.filter_region = 0; g_list.sort = SORT_DATE; list_rebuild_view(); assert(!strcmp(list_view_item(0)->name, "SONIC ADVENTURE"));
    list_toggle_favorite(list_view_item(0)); assert(settings_get()->favorites[2 / 32] & (1u << 2));
    g_list.only_favorites = 1; list_rebuild_view(); assert(g_list.view_count == 1 && list_view_item(0)->slot == 2);
    settings_push_recent(5); settings_push_recent(2); settings_push_recent(5);
    assert(settings_get()->recent[0] == 5 && settings_get()->recent[1] == 2 && settings_get()->launches == 3);
    /* IP.BIN */
    unsigned char hdr[256]; memset(hdr, ' ', sizeof hdr);
    memcpy(hdr, "SEGA SEGAKATANA SEGA ENTERPRISES", 32); memcpy(hdr + 0x20, "2C39 ", 5); memcpy(hdr + 0x25, "GD-ROM1/2  ", 11);
    memcpy(hdr + 0x30, "JU      ", 8); memcpy(hdr + 0x38, "0799A10 ", 8); memcpy(hdr + 0x40, "T-9999N   ", 10); memcpy(hdr + 0x4A, "V1.002", 6);
    memcpy(hdr + 0x50, "19991201", 8); memcpy(hdr + 0x60, "1ST_READ.BIN    ", 16); memcpy(hdr + 0x70, "SEGA ENTERPRISES", 16); memcpy(hdr + 0x80, "SONIC ADVENTURE", 15);
    ipbin_t ip; assert(ipbin_parse(hdr, &ip) == 0);
    assert(!strcmp(ip.name, "SONIC ADVENTURE") && !strcmp(ip.region, "JU") && ip.vga && ip.is_gd && ip.disc_num == '1' && ip.disc_total == '2');
    assert(!strcmp(ip.product, "T9999N") && !strcmp(ip.date, "19991201"));
    puts("list/ini/ipbin OK");
    return 0;
}
