/* Voice messages: record from the Dreamcast microphone (SIP, 8 kHz 8-bit
 * mu-law, the format the hardware produces) and play mu-law buffers on the AICA. */
#pragma once
#include <stdint.h>

#define VOICE_MAX_SEC 12
#define VOICE_RATE 8000
#define VOICE_MAX_BYTES (VOICE_MAX_SEC * VOICE_RATE)

void voice_init(void);
int  voice_mic_present(void);
int  voice_record_start(void);          /* 0 ok */
int  voice_recording(void);
int  voice_record_len(void);            /* bytes so far */
int  voice_record_stop(uint8_t **data, int *len);   /* data valid until next start */
void voice_play(const uint8_t *ulaw, int len);
int  voice_playing(void);
void voice_update(void);                /* once per frame */
int16_t ulaw_decode(uint8_t u);
