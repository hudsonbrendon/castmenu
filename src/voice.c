#include "voice.h"
#include <string.h>
#include <stdlib.h>

int16_t ulaw_decode(uint8_t u) {
    u = ~u;
    int t = ((u & 0x0F) << 3) + 0x84;
    t <<= (u & 0x70) >> 4;
    return (u & 0x80) ? (int16_t)(0x84 - t) : (int16_t)(t - 0x84);
}

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/sip.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>

static uint8_t rec[VOICE_MAX_BYTES];
static volatile int rec_len, recording;
static maple_device_t *mic;
static sfxhnd_t cur_sfx = SFXHND_INVALID;
static uint64_t play_end_ms;

static void sample_cb(maple_device_t *dev, uint8_t *samples, size_t len) {
    (void)dev;
    if (!recording) return;
    size_t room = VOICE_MAX_BYTES - rec_len; if (len > room) len = room;
    memcpy(rec + rec_len, samples, len); rec_len += len;
}

void voice_init(void) { sip_init(); snd_init(); }
int voice_mic_present(void) { mic = maple_enum_type(0, MAPLE_FUNC_MICROPHONE); return mic != NULL; }

int voice_record_start(void) {
    if (!voice_mic_present() || recording) return -1;
    sip_set_sample_type(mic, SIP_SAMPLE_8BIT_ULAW);
    sip_set_frequency(mic, SIP_SAMPLE_8KHZ);
    sip_set_gain(mic, SIP_DEFAULT_GAIN);
    rec_len = 0; recording = 1;
    if (sip_start_sampling(mic, sample_cb, true) != MAPLE_EOK) { recording = 0; return -1; }
    return 0;
}
int voice_recording(void) { return recording; }
int voice_record_len(void) { return rec_len; }
int voice_record_stop(uint8_t **data, int *len) {
    if (!recording) return -1;
    recording = 0;
    if (mic) sip_stop_sampling(mic, true);
    *data = rec; *len = rec_len;
    return 0;
}
void voice_play(const uint8_t *ulaw, int len) {
    if (len <= 0) return;
    if (cur_sfx != SFXHND_INVALID) { snd_sfx_unload(cur_sfx); cur_sfx = SFXHND_INVALID; }
    int16_t *pcm = memalign(32, len * 2); if (!pcm) return;
    for (int i = 0; i < len; i++) pcm[i] = ulaw_decode(ulaw[i]);
    cur_sfx = snd_sfx_load_raw_buf((char *)pcm, len * 2, VOICE_RATE, 16, 1);
    free(pcm);
    if (cur_sfx != SFXHND_INVALID) { snd_sfx_play(cur_sfx, 255, 128); play_end_ms = timer_ms_gettime64() + (uint64_t)len * 1000 / VOICE_RATE; }
}
int voice_playing(void) { return cur_sfx != SFXHND_INVALID && timer_ms_gettime64() < play_end_ms; }
void voice_update(void) {
    if (recording && rec_len >= VOICE_MAX_BYTES) { recording = 0; if (mic) sip_stop_sampling(mic, false); }
}
#else
void voice_init(void) {}
int voice_mic_present(void) { return 0; }
int voice_record_start(void) { return -1; }
int voice_recording(void) { return 0; }
int voice_record_len(void) { return 0; }
int voice_record_stop(uint8_t **d, int *l) { (void)d; (void)l; return -1; }
void voice_play(const uint8_t *u, int l) { (void)u; (void)l; }
int voice_playing(void) { return 0; }
void voice_update(void) {}
#endif
