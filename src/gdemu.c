/* GDEMU vendor command transport over the G1 ATA bus.
 * The register map and PACKET sequence are the same the KOS cdrom driver uses;
 * only opcode 0x52 is GDEMU-specific. */
#include "gdemu.h"
#include <string.h>
#include <arch/types.h>
#include <arch/irq.h>

#define G1_ATA_ALTSTATUS   0xA05F7018
#define G1_ATA_DATA        0xA05F7080
#define G1_ATA_FEATURES    0xA05F7084
#define G1_ATA_LBA_MID     0xA05F7090
#define G1_ATA_LBA_HIGH    0xA05F7094
#define G1_ATA_STATUS      0xA05F709C
#define G1_ATA_COMMAND     0xA05F709C
#define EXT_INT_STAT       0xA05F6904

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01
#define ATAPI_PACKET 0xA0

#define IN8(a)   (*(volatile uint8_t *)(a))
#define IN16(a)  (*(volatile uint16_t *)(a))
#define IN32(a)  (*(volatile uint32_t *)(a))
#define OUT8(a,v)  (*(volatile uint8_t *)(a) = (v))
#define OUT16(a,v) (*(volatile uint16_t *)(a) = (v))

/* ponytail: busy-wait with a bounded spin; GDEMU answers in microseconds. */
static int wait_clear(uint8_t mask) {
    for (uint32_t i = 0; i < 4000000; i++) if (!(IN8(G1_ATA_ALTSTATUS) & mask)) return 0;
    return -1;
}
static int wait_set(uint8_t mask) {
    for (uint32_t i = 0; i < 4000000; i++) if (IN8(G1_ATA_ALTSTATUS) & mask) return 0;
    return -1;
}
static int wait_irq(void) {
    for (uint32_t i = 0; i < 4000000; i++) if (IN32(EXT_INT_STAT) & 1) return 0;
    return -1;
}

static int packet(const uint8_t *cmd12, void *data, uint32_t *size) {
    uint16_t buf[6]; memcpy(buf, cmd12, 12);
    int old = irq_disable();
    if (wait_clear(ATA_SR_BSY | ATA_SR_DRQ)) { irq_restore(old); return -1; }
    OUT8(G1_ATA_FEATURES, 0);
    OUT8(G1_ATA_COMMAND, ATAPI_PACKET);
    if (wait_set(ATA_SR_DRQ)) { irq_restore(old); return -1; }
    (void)IN32(G1_ATA_STATUS);
    for (int i = 0; i < 6; i++) OUT16(G1_ATA_DATA, buf[i]);
    if (wait_irq()) { irq_restore(old); return -1; }
    if (data) {
        if (IN8(G1_ATA_STATUS) & ATA_SR_DRQ) {
            uint16_t len = IN8(G1_ATA_LBA_MID) | (IN8(G1_ATA_LBA_HIGH) << 8);
            uint16_t *d = data; uint32_t cap = *size;
            (void)IN32(G1_ATA_STATUS);
            for (uint16_t i = 0; i < len / 2; i++) { uint16_t w = IN16(G1_ATA_DATA); if ((uint32_t)i * 2 < cap) d[i] = w; }
            *size = len;
            wait_irq();
        } else *size = 0;
    }
    (void)IN32(G1_ATA_STATUS);
    int err = IN8(G1_ATA_ALTSTATUS) & ATA_SR_ERR;
    irq_restore(old);
    return err;
}

int gdemu_get_version(gdemu_version_t *out) {
    uint8_t cmd[12] = { GDEMU_CMD_VENDOR };
    uint8_t raw[8] = {0}; uint32_t size = sizeof raw;
    memset(out, 0, sizeof *out);
    if (packet(cmd, raw, &size) || size < 8) return -1;
    out->bootloader = raw[0] | raw[1] << 8 | raw[2] << 16 | raw[3] << 24;
    out->firmware   = raw[4] | raw[5] << 8 | raw[6] << 16 | raw[7] << 24;
    out->fw_major = raw[7]; out->fw_minor = raw[6]; out->fw_patch = raw[5];
    out->valid = 1;
    return 0;
}

int gdemu_img_step(uint8_t dir) {
    uint8_t cmd[12] = { GDEMU_CMD_VENDOR, GDEMU_SUB_IMG_STEP, dir };
    return packet(cmd, 0, 0);
}

int gdemu_img_set(uint16_t slot) {
    uint8_t cmd[12] = { GDEMU_CMD_VENDOR, GDEMU_SUB_IMG_SET, (uint8_t)slot, (uint8_t)(slot >> 8) };
    return packet(cmd, 0, 0);
}

int gdemu_present(void) {
    gdemu_version_t v; return gdemu_get_version(&v) == 0 && v.valid;
}
