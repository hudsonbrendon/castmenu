#include "proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _arch_dreamcast
#include <kos.h>
#define SET_NONBLOCK(fd) fs_fcntl(fd, F_SETFL, O_NONBLOCK)
#define SOCK_CLOSE(fd) close(fd)
uint32_t proto_now_ms(void) { return (uint32_t)timer_ms_gettime64(); }
#else
#include <time.h>
#define SET_NONBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK)
#define SOCK_CLOSE(fd) close(fd)
uint32_t proto_now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
#endif

static int sock = -1;
static char *rx; static int rx_len;
static int rx_overflow;

/* ---------- base64 ---------- */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int b64_encode(const uint8_t *in, int len, char *out, int cap) {
    int o = 0;
    for (int i = 0; i < len; i += 3) {
        if (o + 5 > cap) return -1;
        uint32_t v = in[i] << 16 | (i + 1 < len ? in[i + 1] << 8 : 0) | (i + 2 < len ? in[i + 2] : 0);
        out[o++] = B64[v >> 18 & 63]; out[o++] = B64[v >> 12 & 63];
        out[o++] = i + 1 < len ? B64[v >> 6 & 63] : '='; out[o++] = i + 2 < len ? B64[v & 63] : '=';
    }
    out[o] = 0; return o;
}
static int b64v(char c) { const char *p = strchr(B64, c); return c && p ? (int)(p - B64) : -1; }
int b64_decode(const char *in, uint8_t *out, int cap) {
    int o = 0, n = strlen(in);
    for (int i = 0; i + 3 < n; i += 4) {
        int a = b64v(in[i]), b = b64v(in[i + 1]), c = in[i + 2] == '=' ? -2 : b64v(in[i + 2]), d = in[i + 3] == '=' ? -2 : b64v(in[i + 3]);
        if (a < 0 || b < 0 || c == -1 || d == -1) return -1;
        if (o >= cap) return -1; out[o++] = a << 2 | b >> 4;
        if (c >= 0) { if (o >= cap) return -1; out[o++] = (b & 15) << 4 | c >> 2; }
        if (d >= 0) { if (o >= cap) return -1; out[o++] = (c & 3) << 6 | d; }
    }
    return o;
}

/* ---------- socket ---------- */
int proto_connect(const char *host, int port, const char *nick, const char *lang) {
    proto_close();
    struct sockaddr_in sa; memset(&sa, 0, sizeof sa); sa.sin_family = AF_INET; sa.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) return -1;
        memcpy(&sa.sin_addr, he->h_addr_list[0], 4);
    }
    sock = socket(AF_INET, SOCK_STREAM, 0); if (sock < 0) return -1;
    if (connect(sock, (struct sockaddr *)&sa, sizeof sa) < 0) { SOCK_CLOSE(sock); sock = -1; return -1; }
    SET_NONBLOCK(sock);
    if (!rx) rx = malloc(PROTO_LINE_MAX);
    rx_len = 0; rx_overflow = 0;
    proto_send("HELLO %s %s 1", nick, lang);
    return 0;
}
void proto_close(void) { if (sock >= 0) SOCK_CLOSE(sock); sock = -1; }
int proto_connected(void) { return sock >= 0; }

int proto_send(const char *fmt, ...) {
    if (sock < 0) return -1;
    char buf[PROTO_TEXT_MAX + 80]; va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf - 1, fmt, ap); va_end(ap);
    if (n < 0) return -1; if (n > (int)sizeof buf - 2) n = sizeof buf - 2;
    buf[n++] = '\n';
    int off = 0;
    while (off < n) { int r = send(sock, buf + off, n - off, 0); if (r < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) continue; return -1; } off += r; }
    return 0;
}
int proto_send_blob(const char *cmd, const char *to, const uint8_t *data, int len) {
    if (sock < 0) return -1;
    int cap = len * 4 / 3 + 80 + (int)strlen(cmd) + (to ? (int)strlen(to) : 0);
    char *line = malloc(cap); if (!line) return -1;
    int n = to ? snprintf(line, cap, "%s %s ", cmd, to) : snprintf(line, cap, "%s ", cmd);
    int e = b64_encode(data, len, line + n, cap - n - 2); if (e < 0) { free(line); return -1; }
    n += e; line[n++] = '\n';
    int off = 0, ok = 0;
    while (off < n) { int r = send(sock, line + off, n - off, 0); if (r < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) continue; ok = -1; break; } off += r; }
    free(line); return ok;
}

/* parse one complete line into an event */
static void split2(char *line, char **a, char **rest) {
    *a = line; char *sp = strchr(line, ' ');
    if (sp) { *sp = 0; *rest = sp + 1; } else *rest = line + strlen(line);
}
static int parse(char *line, proto_event_t *ev) {
    memset(ev, 0, sizeof *ev);
    char *cmd, *rest; split2(line, &cmd, &rest);
    char *a, *text;
#define TXT(dst, src) strncpy(dst, src, sizeof dst - 1)
    if (!strcmp(cmd, "WELCOME")) { split2(rest, &a, &text); ev->type = PEV_WELCOME; TXT(ev->a, a); TXT(ev->text, text); }
    else if (!strcmp(cmd, "PONG")) { ev->type = PEV_PONG; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "ROOMS")) { ev->type = PEV_ROOMS; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "JOINED")) { ev->type = PEV_JOINED; TXT(ev->a, rest); }
    else if (!strcmp(cmd, "MSG")) { split2(rest, &a, &text); ev->type = PEV_MSG; TXT(ev->a, a); char *from, *t2; split2(text, &from, &t2); snprintf(ev->text, sizeof ev->text, "%s\x01%s", from, t2); }
    else if (!strcmp(cmd, "PM")) { split2(rest, &a, &text); ev->type = PEV_PM; TXT(ev->a, a); TXT(ev->text, text); }
    else if (!strcmp(cmd, "VOICE")) {
        split2(rest, &a, &text); ev->type = PEV_VOICE; TXT(ev->a, a);
        int cap = strlen(text) * 3 / 4 + 4; ev->blob = malloc(cap);
        ev->blob_len = ev->blob ? b64_decode(text, ev->blob, cap) : -1;
        if (ev->blob_len < 0) { free(ev->blob); ev->blob = NULL; ev->blob_len = 0; }
    }
    else if (!strcmp(cmd, "FRIENDS")) { ev->type = PEV_FRIENDS; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "PRESENCE")) { split2(rest, &a, &text); ev->type = PEV_PRESENCE; TXT(ev->a, a); TXT(ev->text, text); }
    else if (!strcmp(cmd, "AI")) { ev->type = PEV_AI; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "WHO")) { ev->type = PEV_WHO; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "ERR")) { ev->type = PEV_ERR; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "HISTORY_END")) { ev->type = PEV_HISTORY_END; }
    else if (!strcmp(cmd, "TIME")) { ev->type = PEV_TIME; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "VMULIST")) { ev->type = PEV_VMULIST; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "DCNOW")) { ev->type = PEV_DCNOW; TXT(ev->text, rest); }
    else if (!strcmp(cmd, "VMUDATA")) {
        split2(rest, &a, &text); ev->type = PEV_VMUDATA; TXT(ev->a, a);
        int cap = (int)strlen(text) * 3 / 4 + 4; ev->blob = malloc(cap); ev->blob_len = ev->blob ? b64_decode(text, ev->blob, cap) : 0;
    }
    else return 0;
#undef TXT
    return 1;
}

int proto_poll(proto_event_t *ev) {
    if (sock < 0 || !rx) return 0;
    /* deliver a buffered line first */
    for (;;) {
        char *nl = memchr(rx, '\n', rx_len);
        if (nl) {
            *nl = 0; int line_len = nl - rx + 1;
            int got = rx_overflow ? 0 : parse(rx, ev);
            rx_overflow = 0;
            memmove(rx, rx + line_len, rx_len - line_len); rx_len -= line_len;
            if (got) return 1;
            continue;
        }
        int space = PROTO_LINE_MAX - rx_len - 1;
        if (space <= 0) { rx_len = 0; rx_overflow = 1; space = PROTO_LINE_MAX - 1; }
        int r = recv(sock, rx + rx_len, space, 0);
        if (r > 0) { rx_len += r; continue; }
        if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { proto_close(); memset(ev, 0, sizeof *ev); ev->type = PEV_DISCONNECTED; return 1; }
        return 0;
    }
}
