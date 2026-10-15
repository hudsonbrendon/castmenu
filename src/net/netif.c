#include "netif.h"
#include <string.h>
#include <stdio.h>

static net_status_t st = { NET_IDLE, "", "", -1 };

#ifdef _arch_dreamcast
#include <kos.h>
#include <kos/net.h>
#include <dc/net/broadband_adapter.h>
#include <dc/net/lan_adapter.h>
#include <ppp/ppp.h>
#include <dc/modem/modem.h>

extern int net_dhcp_request(uint32_t required_address);

static void *connect_thread(void *arg) {
    (void)arg;
    st.state = NET_CONNECTING;
    /* 1. Ethernet (INIT_NET already probed bba/la and registered net_default_dev) */
    if (net_default_dev && (net_default_dev->flags & NETIF_RUNNING)) {
        strncpy(st.iface, strstr(net_default_dev->descr ? net_default_dev->descr : "", "LAN") ? "lan" : "bba", sizeof st.iface - 1);
        if (net_dhcp_request(0) == 0 || net_default_dev->ip_addr[0]) {
            snprintf(st.ip, sizeof st.ip, "%d.%d.%d.%d", net_default_dev->ip_addr[0], net_default_dev->ip_addr[1], net_default_dev->ip_addr[2], net_default_dev->ip_addr[3]);
            st.state = NET_UP; return NULL;
        }
        st.state = NET_FAILED; return NULL;
    }
    /* 2. Modem via DreamPi (any number works, PAP login dream/dreamcast) */
    if (modem_init() == 0) {
        strcpy(st.iface, "ppp");
        if (ppp_init() == 0 && ppp_modem_init("111", 0, NULL) == 0) {
            ppp_set_login("dream", "dreamcast");
            if (ppp_connect() == 0 && net_default_dev) {
                snprintf(st.ip, sizeof st.ip, "%d.%d.%d.%d", net_default_dev->ip_addr[0], net_default_dev->ip_addr[1], net_default_dev->ip_addr[2], net_default_dev->ip_addr[3]);
                st.state = NET_UP; return NULL;
            }
        }
        st.state = NET_FAILED; return NULL;
    }
    st.state = NET_NO_ADAPTER;
    return NULL;
}

void net_start(void) {
    if (st.state != NET_IDLE && st.state != NET_FAILED && st.state != NET_NO_ADAPTER) return;
    st.state = NET_CONNECTING;
    thd_create(1, connect_thread, NULL);
}
#else
void net_start(void) { st.state = NET_NO_ADAPTER; }
#endif

net_status_t net_status(void) { return st; }
void net_set_ping(int ms) { st.ping_ms = ms; }
