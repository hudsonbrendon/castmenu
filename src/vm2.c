#include "vm2.h"
#include <kos.h>
#include <kos/genwait.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <arch/rtc.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static void reply(maple_state_t *st, maple_frame_t *frm) { (void)st; genwait_wake_all(frm); }

/* one raw command on a device's frame; returns the response code, data copied to out (max bytes) */
static int docmd(maple_device_t *dev, int cmd, const void *send, int words, uint8_t *out, int max) {
    if (maple_frame_lock(&dev->frame) < 0) return MAPLE_RESPONSE_AGAIN;
    maple_frame_init(&dev->frame);
    uint32_t *sb = (uint32_t *)dev->frame.recv_buf;
    if (words) memcpy(sb, send, words * 4);
    dev->frame.cmd = cmd; dev->frame.dst_port = dev->port; dev->frame.dst_unit = dev->unit;
    dev->frame.length = words; dev->frame.callback = reply; dev->frame.send_buf = sb;
    maple_queue_frame(&dev->frame);
    if (genwait_wait(&dev->frame, "vm2", 200) < 0) { dev->frame.state = MAPLE_FRAME_VACANT; return MAPLE_RESPONSE_NONE; }
    maple_response_t *r = (maple_response_t *)dev->frame.recv_buf;
    int code = r->response, n = r->data_len * 4; if (n > max) n = max;
    if (out && n > 0) memcpy(out, r->data, n);
    dev->frame.state = MAPLE_FRAME_VACANT;
    return code;
}

static int is_virtual(maple_device_t *dev) {
    uint8_t info[160]; memset(info, 0, sizeof info);
    if (docmd(dev, MAPLE_COMMAND_ALLINFO, NULL, 0, info, sizeof info) != MAPLE_RESPONSE_ALLINFO) return 0;
    const char *ext = (const char *)info + 112;   /* after the 112-byte device info block */
    static const char *ids[] = { "VM2 by Dreamware", "USB RP2040 EMU  ", "8BITMODS VMUPro ", "Pico2Maple USBBT" };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) if (!strncasecmp(ext, ids[i], 16)) return 1;
    return 0;
}

int vm2_send_id(const char *product, const char *name) {
    int sent = 0;
    for (int i = 0; i < 8; i++) {
        maple_device_t *dev = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
        if (!dev || !is_virtual(dev)) continue;
        uint8_t buf[144]; memset(buf, 0, sizeof buf);
        uint32_t func = MAPLE_FUNC_MEMCARD; memcpy(buf, &func, 4);
        strncpy((char *)buf + 4, product, 12); if (name) strncpy((char *)buf + 16, name, 128);
        for (int tries = 0; tries < 3; tries++) {
            int r = docmd(dev, 33, buf, name ? 36 : 4, NULL, 0);
            if (r == MAPLE_RESPONSE_OK) { sent++; break; }
            if (r != MAPLE_RESPONSE_AGAIN) break;
            thd_sleep(20);
        }
    }
    return sent;
}

int vmu_clock_sync(void) {
    time_t now = rtc_unix_secs();
    if (now > 978307200) return 0;                 /* 2001-01-01: the console clock is already set */
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CLOCK);
    time_t t = 0;
    if (!dev || vmu_get_datetime(dev, &t) < 0 || t <= 978307200) return 0;
    rtc_set_unix_secs(t);
    return 1;
}
