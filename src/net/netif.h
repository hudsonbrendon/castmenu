/* Network bring-up: BBA / LAN adapter (DHCP) first, then modem PPP via a
 * DreamPi-style bridge ("Wi-Fi"). Runs in a thread; poll net_status(). */
#pragma once
#include <stdint.h>

typedef enum { NET_IDLE, NET_CONNECTING, NET_UP, NET_FAILED, NET_NO_ADAPTER } net_state_t;
typedef struct {
    net_state_t state;
    char iface[8];     /* "bba", "lan", "ppp" */
    char ip[16];
    int  ping_ms;      /* last server round trip, -1 unknown */
} net_status_t;

void net_start(void);            /* idempotent; spawns the connect thread */
net_status_t net_status(void);
void net_set_ping(int ms);
