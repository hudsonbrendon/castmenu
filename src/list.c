#include "list.h"
#include "ini.h"
#include "ipbin.h"
#include "gdemu.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/cdrom.h>
#endif

gamelist_t g_list;

static game_t *slot_item(int slot) {
    for (int i = 0; i < g_list.count; i++) if (g_list.items[i].slot == slot) return &g_list.items[i];
    if (g_list.count >= LIST_MAX) return NULL;
    game_t *g = &g_list.items[g_list.count++];
    memset(g, 0, sizeof *g); g->slot = slot;
    return g;
}

static void copy_field(char *dst, int cap, const char *v) { strncpy(dst, v, cap - 1); dst[cap - 1] = 0; }

/* "NN.key=value" lines under [GDMENU] or [CASTMENU] */
static int list_ini_cb(void *user, const char *section, const char *key, const char *value) {
    (void)user;
    const char *dot = strchr(key, '.');
    if (!dot) {
        if (!strcasecmp(key, "server") && value[0]) { settings_t *st = settings_get(); if (!st->server_user) { strncpy(st->server, value, SERVER_LEN - 1); st->server[SERVER_LEN - 1] = 0; } }
        return 1;
    }
    int slot = atoi(key);
    if (slot < 1 || slot > LIST_MAX) return 1;
    game_t *g = slot_item(slot); if (!g) return 0;
    const char *k = dot + 1;
    if (!strcasecmp(k, "name")) copy_field(g->name, sizeof g->name, value);
    else if (!strcasecmp(k, "disc")) copy_field(g->disc, sizeof g->disc, value);
    else if (!strcasecmp(k, "region")) copy_field(g->region, sizeof g->region, value);
    else if (!strcasecmp(k, "version")) copy_field(g->version, sizeof g->version, value);
    else if (!strcasecmp(k, "date")) copy_field(g->date, sizeof g->date, value);
    else if (!strcasecmp(k, "product")) copy_field(g->product, sizeof g->product, value);
    else if (!strcasecmp(k, "genre")) copy_field(g->genre, sizeof g->genre, value);
    else if (!strcasecmp(k, "year")) copy_field(g->year, sizeof g->year, value);
    else if (!strcasecmp(k, "publisher")) copy_field(g->publisher, sizeof g->publisher, value);
    else if (!strcasecmp(k, "category")) {
        int c = 0;
        for (int i = 0; i < g_list.cat_count; i++) if (!strcasecmp(g_list.cat_names[i], value)) c = i + 1;
        if (!c && g_list.cat_count < CAT_MAX) { copy_field(g_list.cat_names[g_list.cat_count++], CAT_NAME_LEN, value); c = g_list.cat_count; }
        g->category = c;
    }
    else if (!strcasecmp(k, "vga")) g->vga = atoi(value) != 0;
    else if (!strcasecmp(k, "gd")) g->is_gd = atoi(value) != 0;
    else if (!strcasecmp(k, "players")) g->players = atoi(value);
    else if (!strcasecmp(k, "cover")) g->has_cover = atoi(value) != 0;
    else if (!strcasecmp(k, "hidden")) g->hidden = atoi(value) != 0;
    return 1;
}

#ifdef _arch_dreamcast
/* Slow path: ask GDEMU to mount each slot and read its IP.BIN, like GDMENU
 * does when LIST.INI is absent. Stops at the first empty slot. */
static int scan_slots(void (*progress)(int, const char *)) {
    unsigned char sec[2048] __attribute__((aligned(32)));
    char msg[64];
    for (int slot = 2; slot <= LIST_MAX; slot++) {
        snprintf(msg, sizeof msg, "Lendo pasta %02d", slot);
        if (progress) progress(slot * 100 / 200, msg);
        if (gdemu_img_set(slot)) break;
        thd_sleep(settings_get()->scan_delay_ms);
        if (cdrom_reinit() != ERR_OK) break;
        cd_toc_t toc;
        /* GD-ROM: IP.BIN lives on the high-density area; CD (MIL-CD) images only have a low-density TOC */
        if (cdrom_read_toc(&toc, true) != ERR_OK && cdrom_read_toc(&toc, false) != ERR_OK) break; /* empty folder */
        int lba = -1;
        int first = TOC_TRACK(toc.first), last = TOC_TRACK(toc.last);
        for (int t = first; t <= last && t <= 99; t++)
            if (TOC_CTRL(toc.entry[t - 1]) & 4) lba = TOC_LBA(toc.entry[t - 1]); /* last data track */
        if (lba < 0) break;
        if (cdrom_read_sectors(sec, lba, 1) != ERR_OK) break;
        ipbin_t ip;
        if (ipbin_parse(sec, &ip)) { game_t *g = slot_item(slot); if (g) strcpy(g->name, "DISCO NAO SUPORTADO"); continue; }
        game_t *g = slot_item(slot); if (!g) break;
        copy_field(g->name, sizeof g->name, ip.name);
        if (ip.disc_num) snprintf(g->disc, sizeof g->disc, "%c/%c", ip.disc_num, ip.disc_total);
        copy_field(g->region, sizeof g->region, ip.region);
        copy_field(g->version, sizeof g->version, ip.version);
        copy_field(g->date, sizeof g->date, ip.date);
        copy_field(g->product, sizeof g->product, ip.product);
        g->vga = ip.vga; g->is_gd = ip.is_gd;
    }
    gdemu_img_set(1);
    thd_sleep(300);
    cdrom_reinit();
    return g_list.count;
}
#endif

int list_ini_cb_for_test(void *u, const char *s, const char *k, const char *v) { return list_ini_cb(u, s, k, v); }

int list_load(void (*progress)(int, const char *)) {
    memset(&g_list, 0, sizeof g_list);
    g_list.hide_multidisc_extra = 1;
    int n = ini_parse_file("/cd/LIST.INI", list_ini_cb, NULL);
    if (n < 0) n = ini_parse_file("/cd/list.ini", list_ini_cb, NULL);
    ini_parse_file("/cd/CASTMENU.INI", list_ini_cb, NULL);
#ifdef _arch_dreamcast
    if (n <= 0) scan_slots(progress);
#else
    (void)progress;
#endif
    /* drop the menu itself (slot 1) */
    for (int i = 0; i < g_list.count; i++) if (g_list.items[i].slot == 1) g_list.items[i].hidden = 1;
    settings_apply_favorites(&g_list);
    list_rebuild_view();
    return g_list.count;
}

static int cmp_name(const void *a, const void *b) {
    return strcasecmp(g_list.items[*(const uint16_t *)a].name, g_list.items[*(const uint16_t *)b].name);
}
static int cmp_date(const void *a, const void *b) {
    int r = strcmp(g_list.items[*(const uint16_t *)a].date, g_list.items[*(const uint16_t *)b].date);
    return r ? r : cmp_name(a, b);
}
static int cmp_region(const void *a, const void *b) {
    int r = strcmp(g_list.items[*(const uint16_t *)a].region, g_list.items[*(const uint16_t *)b].region);
    return r ? r : cmp_name(a, b);
}
static int cmp_slot(const void *a, const void *b) {
    return (int)g_list.items[*(const uint16_t *)a].slot - (int)g_list.items[*(const uint16_t *)b].slot;
}

void list_rebuild_view(void) {
    g_list.view_count = 0;
    if (g_list.only_recent) {
        const settings_t *s = settings_get();
        for (unsigned r = 0; r < sizeof s->recent / sizeof s->recent[0]; r++)
            for (int i = 0; i < g_list.count; i++)
                if (s->recent[r] && g_list.items[i].slot == s->recent[r] && !g_list.items[i].hidden) g_list.view[g_list.view_count++] = i;
        return;
    }
    for (int i = 0; i < g_list.count; i++) {
        game_t *g = &g_list.items[i];
        if (g->hidden) continue;
        if (g_list.hide_multidisc_extra && g->disc[0] > '1' && g->disc[2] > '1') continue;
        if (g_list.only_favorites && !g->favorite) continue;
        if (g_list.category >= CAT_FIRST_NAMED && g->category != g_list.category - CAT_FIRST_NAMED + 1) continue;
        if (g_list.filter_region && !strchr(g->region, g_list.filter_region)) continue;
        if (g_list.filter_letter) {
            char c = toupper((unsigned char)g->name[0]);
            if (g_list.filter_letter == '#' ? isalpha((unsigned char)c) : c != g_list.filter_letter) continue;
        }
        g_list.view[g_list.view_count++] = i;
    }
    static int (*const cmps[SORT_COUNT])(const void *, const void *) = { cmp_slot, cmp_name, cmp_date, cmp_region };
    qsort(g_list.view, g_list.view_count, sizeof g_list.view[0], cmps[g_list.sort]);
}

game_t *list_view_item(int idx) {
    if (idx < 0 || idx >= g_list.view_count) return NULL;
    return &g_list.items[g_list.view[idx]];
}

int list_view_index_of_slot(uint16_t slot) {
    for (int i = 0; i < g_list.view_count; i++) if (g_list.items[g_list.view[i]].slot == slot) return i;
    return -1;
}

int list_disc_set(const game_t *g, game_t **out, int max) {
    int n = 0;
    if (!g->product[0] || !g->disc[0]) return 0;
    for (int i = 0; i < g_list.count && n < max; i++) {
        game_t *o = &g_list.items[i];
        if (!strcmp(o->product, g->product) && o->disc[2] == g->disc[2]) out[n++] = o;
    }
    /* order by disc number */
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && out[j - 1]->disc[0] > out[j]->disc[0]; j--) { game_t *t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

void list_toggle_favorite(game_t *g) {
    g->favorite = !g->favorite;
    settings_set_favorite(g->slot, g->favorite);
}

int list_category_total(void) { return CAT_FIRST_NAMED + g_list.cat_count; }
const char *list_category_name(int cat) {
    if (cat >= CAT_FIRST_NAMED && cat - CAT_FIRST_NAMED < g_list.cat_count) return g_list.cat_names[cat - CAT_FIRST_NAMED];
    return cat == CAT_FAVORITES ? "FAVORITES" : cat == CAT_RECENT ? "RECENT" : "ALL"; /* UI maps these through tr() */
}
void list_set_category(int cat) {
    int n = list_category_total(); if (n <= 0) n = 1;
    g_list.category = ((cat % n) + n) % n;
    g_list.only_favorites = g_list.category == CAT_FAVORITES;
    g_list.only_recent = g_list.category == CAT_RECENT;
    list_rebuild_view();
}
int list_synopsis(const game_t *g, const char *lang, char *out, int cap) {
    char path[48]; out[0] = 0;
    const char *dirs[2] = { lang, "any" };
    for (int d = 0; d < 2; d++) {
        snprintf(path, sizeof path, "/cd/info/%s/%03d.txt", dirs[d], g->slot);
        FILE *f = fopen(path, "rb"); if (!f) continue;
        size_t n = fread(out, 1, cap - 1, f); fclose(f); out[n] = 0;
        for (char *p = out; *p; p++) if (*p == '\r') *p = ' ';
        return 0;
    }
    return -1;
}

const char *list_sort_name(sort_mode_t m) {
    static const char *n[SORT_COUNT] = { "PASTA", "NOME", "DATA", "REGIAO" };
    return n[m];
}
