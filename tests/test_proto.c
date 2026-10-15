/* Host test for src/net/proto.c against server/cast_server.py.
 * usage: test_proto <port>  (tests/test_proto.py starts the server and runs this) */
#include "../src/net/proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int wait_ev(proto_ev_t want, proto_event_t *ev, int ms) {
    for (int i = 0; i < ms / 10; i++) { while (proto_poll(ev)) { if (ev->type == want) return 1; if (ev->blob) free(ev->blob); } usleep(10000); }
    return 0;
}
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 7350;
    proto_event_t ev;
    /* base64 round trip */
    uint8_t raw[300]; for (int i = 0; i < 300; i++) raw[i] = (uint8_t)(i * 7);
    char b64[500]; uint8_t back[300];
    CHECK(b64_encode(raw, 300, b64, sizeof b64) == 400);
    CHECK(b64_decode(b64, back, 300) == 300 && !memcmp(raw, back, 300));
    CHECK(b64_encode(raw, 1, b64, sizeof b64) == 4 && b64_decode(b64, back, 300) == 1 && back[0] == raw[0]);
    /* connect + hello */
    CHECK(proto_connect("127.0.0.1", port, "Tester", "pt") == 0);
    CHECK(wait_ev(PEV_WELCOME, &ev, 2000)); CHECK(!strcmp(ev.a, "Tester"));
    proto_send("ROOMS"); CHECK(wait_ev(PEV_ROOMS, &ev, 2000)); CHECK(strstr(ev.text, "geral:"));
    proto_send("PING 123"); CHECK(wait_ev(PEV_PONG, &ev, 2000)); CHECK(!strcmp(ev.text, "123"));
    proto_send("JOIN geral"); CHECK(wait_ev(PEV_JOINED, &ev, 2000)); CHECK(!strcmp(ev.a, "geral"));
    proto_send("SAY geral ola mundo"); CHECK(wait_ev(PEV_MSG, &ev, 2000)); CHECK(!strcmp(ev.a, "geral")); CHECK(!strcmp(ev.text, "Tester\x01ola mundo"));
    proto_send("ADD Amigo"); CHECK(wait_ev(PEV_FRIENDS, &ev, 2000)); CHECK(strstr(ev.text, "Amigo:0"));
    proto_send("PM Amigo oi"); proto_send("HISTORY Amigo"); CHECK(wait_ev(PEV_HISTORY_END, &ev, 2000));
    /* voice to myself: server stores it and relays since I am online */
    uint8_t voice[8000]; for (int i = 0; i < 8000; i++) voice[i] = (uint8_t)i;
    CHECK(proto_send_blob("VOICE", "Tester", voice, 8000) == 0);
    CHECK(wait_ev(PEV_VOICE, &ev, 3000)); CHECK(ev.blob_len == 8000 && !memcmp(ev.blob, voice, 8000)); free(ev.blob);
    proto_send("AI qual o melhor jogo?"); CHECK(wait_ev(PEV_AI, &ev, 3000)); CHECK(strstr(ev.text, "melhor jogo"));
    proto_send("JOIN nope"); CHECK(wait_ev(PEV_ERR, &ev, 2000));
    proto_close(); CHECK(!proto_connected());
    printf("proto OK\n");
    return 0;
}
