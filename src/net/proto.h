/* Cast Chat client protocol (portable: KOS sockets or POSIX for host tests).
 *
 * Line-based TCP, UTF-8 not required (Latin-1 passes through), one message per
 * line, "\n" terminated. Fields are space separated; the last field may contain
 * spaces. Voice is base64 of 8 kHz 8-bit mu-law.
 *
 *  client -> server                       server -> client
 *  HELLO <nick> <lang> <version>          WELCOME <nick> <motd...>
 *  PING <ms-since-boot>                   PONG <same>
 *  ROOMS                                  ROOMS <name:users> <name:users> ...
 *  JOIN <room>                            JOINED <room> | ERR <text>
 *  SAY <room> <text>                      MSG <room> <nick> <text>
 *  FRIENDS                                FRIENDS <nick:1|0> ...   (1 = online)
 *  ADD <nick> / DEL <nick>                FRIENDS ... (refreshed)
 *  PM <nick> <text>                       PM <from> <text>
 *  VOICE <nick> <base64>                  VOICE <from> <base64>
 *  HISTORY <nick|room>                    MSG/PM lines replayed, then HISTORY_END
 *  AI <text>                              AI <text>
 *  AIVOICE <base64>                       AI <text>   (server transcribes)
 *  WHO                                    WHO <nick> ...
 *  TIME                                   TIME <unix seconds>   (console clock sync)
 *  VMUPUT <file> <base64>                 VMULIST ...           (cloud backup of a VMU save)
 *  VMULIST                                VMULIST <file:bytes> ...
 *  VMUGET <file>                          VMUDATA <file> <base64>
 *  VMUDEL <file>                          VMULIST ...
 *  DCNOW                                  DCNOW <online> <nick:game> ...   (dreamcast.online/now, read-only)
 *                                         PRESENCE <nick> 1|0
 *                                         ERR <text>
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define PROTO_LINE_MAX 262144   /* voice lines are big */
#define PROTO_TEXT_MAX 240

typedef enum { PEV_NONE, PEV_WELCOME, PEV_PONG, PEV_ROOMS, PEV_JOINED, PEV_MSG, PEV_PM, PEV_VOICE, PEV_FRIENDS, PEV_PRESENCE, PEV_AI, PEV_WHO, PEV_ERR, PEV_HISTORY_END, PEV_DISCONNECTED, PEV_TIME, PEV_VMULIST, PEV_VMUDATA, PEV_DCNOW } proto_ev_t;

typedef struct {
    proto_ev_t type;
    char a[48];                 /* room / from / nick */
    char text[PROTO_TEXT_MAX];  /* message text or list payload */
    uint8_t *blob; int blob_len;/* decoded voice (malloc, owner = consumer) */
} proto_event_t;

int  proto_connect(const char *host, int port, const char *nick, const char *lang);  /* 0 ok (socket non-blocking) */
void proto_close(void);
int  proto_connected(void);
int  proto_poll(proto_event_t *ev);           /* 1 if an event was produced */
int  proto_send(const char *fmt, ...);        /* appends "\n" */
int  proto_send_blob(const char *cmd, const char *to, const uint8_t *data, int len); /* base64 line */
uint32_t proto_now_ms(void);

/* helpers (also used by tests) */
int b64_encode(const uint8_t *in, int len, char *out, int cap);
int b64_decode(const char *in, uint8_t *out, int cap);
