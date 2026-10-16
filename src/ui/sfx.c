#include "sfx.h"
#include "../settings.h"
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <stdio.h>

static sfxhnd_t h[SFX_COUNT]; static const char *names[SFX_COUNT] = { "move", "select", "back" };

static void load(const char *dir) {
    for (int i = 0; i < SFX_COUNT; i++) {
        if (h[i] != SFXHND_INVALID) snd_sfx_unload(h[i]);
        char p[96]; snprintf(p, sizeof p, "%s/%s.wav", dir, names[i]);
        file_t f = fs_open(p, O_RDONLY);                     /* probe first: snd_sfx_load logs every miss */
        if (f < 0) snprintf(p, sizeof p, "/rd/sfx/%s.wav", names[i]); else fs_close(f);
        h[i] = snd_sfx_load(p);
    }
}
void sfx_init(void) { snd_init(); for (int i = 0; i < SFX_COUNT; i++) h[i] = SFXHND_INVALID; load("/rd/sfx"); }
void sfx_theme(const char *dir) { load(dir ? dir : "/rd/sfx"); }
void sfx_play(sfx_t s) { if (!settings_get()->no_sfx && h[s] != SFXHND_INVALID) snd_sfx_play(h[s], s == SFX_MOVE ? 120 : 180, 128); }
