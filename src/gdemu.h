/* GDEMU vendor commands (ATAPI PACKET opcode 0x52), as used by GDMENU 0.6.
 * See docs/REVERSE_ENGINEERING.md for the evidence behind each command. */
#pragma once
#include <stdint.h>

#define GDEMU_CMD_VENDOR     0x52
#define GDEMU_SUB_IMG_STEP   0x81  /* param: GDEMU_STEP_NEXT / GDEMU_STEP_PREV */
#define GDEMU_SUB_IMG_SET    0x82  /* param: little-endian u16 slot (0 = default image) */
#define GDEMU_STEP_NEXT      0x55
#define GDEMU_STEP_PREV      0x44

typedef struct {
    uint32_t bootloader; /* raw 00 00 09 01 -> 1.9  (0 on early boards) */
    uint32_t firmware;   /* raw 00 00 14 05 -> 5.20 */
    uint8_t  fw_major, fw_minor, fw_patch;
    int      valid;
} gdemu_version_t;

/* Returns 0 on success, nonzero if the drive flagged ATA_SR_ERR. */
int gdemu_get_version(gdemu_version_t *out);
int gdemu_img_step(uint8_t dir);
int gdemu_img_set(uint16_t slot);
/* 1 if the drive answered the vendor version query (i.e. a GDEMU is present). */
int gdemu_present(void);
