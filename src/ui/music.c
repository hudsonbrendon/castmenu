#include "music.h"
#include "anim.h"
#include "../settings.h"
#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOT 1000
#define MENU_TRACKS 32
#define HOVER_S 0.9f
#define SECTORS 24                                  /* 48 KB ring: a request never spans more */
static struct { uint32_t lba, bytes, rate; } idx[MAX_SLOT];
static struct { uint32_t lba, bytes, rate; } menu[MENU_TRACKS]; static int menu_n, menu_cur;
static snd_stream_hnd_t hnd = SND_STREAM_INVALID;
static uint8_t *buf; static uint32_t cur_lba, cur_bytes, pos; static int playing, loops;
static uint16_t wanted; static float hover; static int menu_mode;

static void *cb(snd_stream_hnd_t h, int req, int *got) {
    (void)h;
    if (!playing || req <= 0) { *got = 0; return buf; }
    if (pos >= cur_bytes) { pos = 0; loops++; }
    uint32_t n = (uint32_t)req; if (n > cur_bytes - pos) n = cur_bytes - pos;
    if (n > (SECTORS - 2) * 2048) n = (SECTORS - 2) * 2048;
    uint32_t first = pos / 2048, last = (pos + n - 1) / 2048;
    if (cdrom_read_sectors(buf, (int)(cur_lba + 1 + first + 150), (int)(last - first + 1)) != ERR_OK) { dbglog(DBG_INFO, "music: read error at %lu\n", (unsigned long)pos); playing = 0; *got = 0; return buf; }
    void *p = buf + (pos % 2048); pos += n; *got = (int)n; return p;
}

void music_init(void) {
    FILE *f = fopen("/cd/music.idx", "r");
    if (f) {
        unsigned s, l, b, r;
        while (fscanf(f, "%u %u %u %u", &s, &l, &b, &r) == 4) {
            if (s < MAX_SLOT && b) { idx[s].lba = l; idx[s].bytes = b; idx[s].rate = r ? r : 22050; }
            else if (s >= 1000 && menu_n < MENU_TRACKS && b) { menu[menu_n].lba = l; menu[menu_n].bytes = b; menu[menu_n].rate = r ? r : 22050; menu_n++; }
        }
        fclose(f);
    }
    if (!idx[0].bytes && !menu_n && !f) return;
    snd_stream_init();
    buf = memalign(32, SECTORS * 2048);
    if (buf) hnd = snd_stream_alloc(cb, 16384);
    dbglog(DBG_INFO, "music: %d menu tracks, stream %d\n", menu_n, (int)hnd);
}
int music_has(uint16_t slot) { return slot < MAX_SLOT && idx[slot].bytes && hnd != SND_STREAM_INVALID; }

static void start(uint32_t lba, uint32_t bytes, uint32_t rate) {
    if (hnd == SND_STREAM_INVALID) return;
    if (playing) snd_stream_stop(hnd);
    cur_lba = lba; cur_bytes = bytes; pos = 0; loops = 0; playing = 1;
    dbglog(DBG_INFO, "music: start lba %lu bytes %lu rate %lu\n", (unsigned long)lba, (unsigned long)bytes, (unsigned long)rate);
    snd_stream_volume(hnd, 200);
    snd_stream_start_adpcm(hnd, rate, 0);
}
void music_stop(void) { if (playing && hnd != SND_STREAM_INVALID) snd_stream_stop(hnd); playing = 0; wanted = 0; menu_mode = 0; }

void music_update(uint16_t slot, float dt) {
    if (hnd == SND_STREAM_INVALID) return;
    settings_t *s = settings_get();
    if (s->no_music || !music_has(slot)) slot = 0;
    if (slot != wanted) { wanted = slot; hover = 0; }
    hover += dt;
    if (wanted && hover > HOVER_S && (!playing || menu_mode || cur_lba != idx[wanted].lba)) { menu_mode = 0; start(idx[wanted].lba, idx[wanted].bytes, idx[wanted].rate); }
    else if (!wanted && hover > HOVER_S && !menu_mode) {
        /* nothing for this game: menu tracks (if any, and enabled), otherwise silence */
        if (playing) { snd_stream_stop(hnd); playing = 0; }
        if (menu_n && !s->no_bgm) { menu_mode = 1; start(menu[menu_cur].lba, menu[menu_cur].bytes, menu[menu_cur].rate); }
    }
    if (menu_mode && playing && loops >= 1 && menu_n > 1) {        /* next menu track after one pass */
        menu_cur = (menu_cur + 1) % menu_n; start(menu[menu_cur].lba, menu[menu_cur].bytes, menu[menu_cur].rate);
    }
    if (menu_mode && s->no_bgm) { snd_stream_stop(hnd); playing = 0; menu_mode = 0; }
    if (playing) snd_stream_poll(hnd);
}
