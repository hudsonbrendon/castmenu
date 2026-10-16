#include "video.h"
#include "draw.h"
#include "theme.h"
#include "anim.h"
#include "../settings.h"
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOT 1000
#define HOVER_S 0.7f
static struct { uint32_t lba; uint16_t frames, fps, w, h; } idx[MAX_SLOT];
static texture_t tex[2]; static int front = -1;
static uint8_t *buf; static size_t frame_bytes; static int frame_secs, frame;
static uint16_t playing, wanted; static float hover, acc, alpha;

void video_init(void) {
    FILE *f = fopen("/cd/videos.idx", "r"); if (!f) return;
    unsigned s, l, n, fps, w, h;
    while (fscanf(f, "%u %u %u %u %u %u", &s, &l, &n, &fps, &w, &h) == 6)
        if (s < MAX_SLOT && n && w && h) { idx[s].lba = l; idx[s].frames = n; idx[s].fps = fps ? fps : 10; idx[s].w = w; idx[s].h = h; }
    fclose(f);
}
int video_has(uint16_t slot) { return slot < MAX_SLOT && idx[slot].frames; }
void video_stop(void) { playing = wanted = 0; frame = 0; alpha = 0; front = -1; }

static int start(uint16_t slot) {
    frame_bytes = 2048 + (size_t)idx[slot].w * idx[slot].h / 4; frame_secs = (frame_bytes + 2047) / 2048;
    if (!buf) buf = memalign(32, 2048 * 10);                      /* up to 256x256 VQ (9 sectors) */
    if (!buf || frame_secs > 10) return -1;
    for (int i = 0; i < 2; i++) {
        if (!tex[i].ptr) tex[i].ptr = pvr_mem_malloc(2048 + 256 * 256 / 4);
        if (!tex[i].ptr) return -1;
        tex[i].w = tex[i].iw = idx[slot].w; tex[i].h = tex[i].ih = idx[slot].h;
        tex[i].fmt = PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED | PVR_TXRFMT_VQ_ENABLE;
    }
    playing = slot; frame = 0; acc = 1; front = -1; alpha = 0; return 0;
}
/* ponytail: synchronous 18 KB sector read per frame (~ms on GDEMU); move to a thread if it ever hitches */
static int load_frame(void) {
    int lba = (int)idx[playing].lba + 1 + frame * frame_secs + 150;   /* +150: the GD-ROM syscall takes MSF-based addresses */
    if (cdrom_read_sectors(buf, lba, frame_secs) != ERR_OK) return -1;
    int back = front < 0 ? 0 : front ^ 1;
    pvr_txr_load(buf, tex[back].ptr, frame_bytes);
    front = back; frame = (frame + 1) % idx[playing].frames; return 0;
}

void video_update(uint16_t slot, float dt) {
    if (settings_get()->no_bg_video || !video_has(slot)) slot = 0;
    if (slot != wanted) { wanted = slot; hover = 0; }
    hover += dt;
    if (playing && playing != wanted) { alpha = clampf(alpha - dt * 3, 0, 1); if (alpha <= 0) { playing = 0; front = -1; } }
    else if (!playing && wanted && hover > HOVER_S && start(wanted)) { idx[wanted].frames = 0; wanted = 0; }
    if (!playing) return;
    acc += dt;
    float step = 1.0f / idx[playing].fps;
    if (acc >= step) { acc = acc > 2 * step ? 0 : acc - step; if (load_frame()) { idx[playing].frames = 0; video_stop(); return; } }
    if (playing == wanted && front >= 0) alpha = clampf(alpha + dt * 1.5f, 0, 1);
}

void video_draw(void) {
    if (!playing || front < 0 || alpha <= 0) return;
    /* light themes (shell white) keep the clip ghosted so dark text stays readable; dark themes let it through dimmed */
    uint32_t bg = g_theme.bg; int lum = ((bg >> 16 & 255) * 3 + (bg >> 8 & 255) * 6 + (bg & 255)) / 10;
    int light = lum > 128;
    draw_tex(&tex[front], 0, 0, SCREEN_W, SCREEN_H, light ? RGBA(255, 255, 255, (int)(alpha * 135)) : RGBA(165, 165, 165, (int)(alpha * 215)));
}
