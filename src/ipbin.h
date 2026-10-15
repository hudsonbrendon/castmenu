/* Dreamcast IP.BIN (first 0x8000 bytes of the boot track) metadata. */
#pragma once
typedef struct {
    char hw[17], maker[17], crc[6], device[7], area[9], periph[9];
    char product[11], version[7], date[9], boot[17], company[17], name[65];
    char disc_num, disc_total;
    char region[4];   /* subset of "JUE" */
    int  vga;         /* periph flag */
    int  is_gd;       /* "GD-ROM1/1" vs "CD-ROM" */
} ipbin_t;
/* Parse 0x100 header bytes. Returns 0 if the SEGA SEGAKATANA signature matches. */
int ipbin_parse(const unsigned char *hdr, ipbin_t *out);
