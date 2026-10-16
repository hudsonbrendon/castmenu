#include "vmu_ui.h"
#include "screens.h"
#include "chrome.h"
#include "anim.h"
#include "../net/proto.h"
#include <kos.h>
#include <dc/vmufs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEV 8
#define MAX_FILES 64
#define MAX_CLOUD 64
static maple_device_t *devs[MAX_DEV]; static int ndev, cur_dev;
static vmu_dir_t files[MAX_FILES]; static int nfiles, sel, free_blocks;
static struct { char fn[16]; int size; } cloud[MAX_CLOUD]; static int ncloud, cloud_mode, cloud_sel;
static int confirm_del; static float busy;

static void rescan(void) {
    ndev = 0; for (int i = 0; i < MAX_DEV; i++) { maple_device_t *d = maple_enum_type(i, MAPLE_FUNC_MEMCARD); if (d) devs[ndev++] = d; }
    if (cur_dev >= ndev) cur_dev = 0;
    nfiles = 0; free_blocks = 0;
    if (!ndev) return;
    vmu_dir_t *dir = NULL; int cnt = 0;
    if (vmufs_readdir(devs[cur_dev], &dir, &cnt) == 0 && dir) {
        for (int i = 0; i < cnt && nfiles < MAX_FILES; i++) if (dir[i].filetype) files[nfiles++] = dir[i];
        free(dir);
    }
    free_blocks = vmufs_free_blocks(devs[cur_dev]);
    if (sel >= nfiles) sel = nfiles ? nfiles - 1 : 0;
}
static void fname(const vmu_dir_t *d, char *out) { memcpy(out, d->filename, 12); out[12] = 0; for (int i = 11; i >= 0 && out[i] == ' '; i--) out[i] = 0; }

void vmu_ui_open(void) { cur_dev = 0; sel = 0; cloud_mode = 0; confirm_del = 0; busy = 0; rescan(); if (chatnet_online()) proto_send("VMULIST"); }

void vmu_ui_on_list(const char *text) {
    ncloud = 0; char buf[400]; strncpy(buf, text, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (char *p = strtok(buf, " "); p && ncloud < MAX_CLOUD; p = strtok(NULL, " ")) {
        char *c = strrchr(p, ':'); if (!c) continue; *c = 0;
        strncpy(cloud[ncloud].fn, p, 15); cloud[ncloud].fn[15] = 0; cloud[ncloud].size = atoi(c + 1); ncloud++;
    }
}
void vmu_ui_on_data(const char *fn, const uint8_t *data, int len) {
    if (!ndev) return;
    int r = vmufs_write(devs[cur_dev], fn, (void *)data, len, VMUFS_OVERWRITE);
    show_toast(r == 0 ? tr(S_VMU_RESTORED) : tr(S_VMU_FAIL), r == 0 ? 2 : 3);
    rescan();
}

static void copy_to_other(void) {
    if (ndev < 2 || !nfiles) { show_toast(tr(S_VMU_NEED_TWO), 3); return; }
    char fn[13]; fname(&files[sel], fn); void *buf = NULL; int size = 0;
    maple_device_t *dst = devs[(cur_dev + 1) % ndev];
    if (vmufs_read(devs[cur_dev], fn, &buf, &size) != 0 || !buf) { show_toast(tr(S_VMU_FAIL), 3); return; }
    int r = vmufs_write(dst, fn, buf, size, VMUFS_OVERWRITE | (files[sel].filetype == 0xCC ? VMUFS_VMUGAME : 0));
    free(buf); show_toast(r == 0 ? tr(S_VMU_COPIED) : tr(S_VMU_FAIL), r == 0 ? 2 : 3);
}
static void upload(void) {
    if (!nfiles) return;
    if (!chatnet_online()) { show_toast(tr(S_OFFLINE), 3); return; }
    char fn[13]; fname(&files[sel], fn); void *buf = NULL; int size = 0;
    if (vmufs_read(devs[cur_dev], fn, &buf, &size) != 0 || !buf) { show_toast(tr(S_VMU_FAIL), 3); return; }
    proto_send_blob("VMUPUT", fn, buf, size); free(buf);
    show_toast(tr(S_VMU_UPLOADED), 2); proto_send("VMULIST");
}

int vmu_ui_update(const input_t *in) {
    if (confirm_del) {
        if (in->pressed & BTN_B) confirm_del = 0;
        if (in->pressed & BTN_A) { char fn[13]; fname(&files[sel], fn); int r = vmufs_delete(devs[cur_dev], fn); show_toast(r == 0 ? tr(S_VMU_DELETED) : tr(S_VMU_FAIL), r == 0 ? 2 : 3); confirm_del = 0; rescan(); }
        return ACT_NONE;
    }
    if (cloud_mode) {
        if (in->repeat & BTN_UP && ncloud) cloud_sel = (cloud_sel + ncloud - 1) % ncloud;
        if (in->repeat & BTN_DOWN && ncloud) cloud_sel = (cloud_sel + 1) % ncloud;
        if (in->pressed & BTN_A && ncloud && ndev) { proto_send("VMUGET %s", cloud[cloud_sel].fn); show_toast(tr(S_LOADING), 0); }
        if (in->pressed & BTN_X && ncloud) { proto_send("VMUDEL %s", cloud[cloud_sel].fn); }
        if (in->pressed & (BTN_B | BTN_START)) cloud_mode = 0;
        return ACT_NONE;
    }
    if (in->pressed & BTN_B) return ACT_CLOSE;
    if (in->repeat & BTN_UP && nfiles) sel = (sel + nfiles - 1) % nfiles;
    if (in->repeat & BTN_DOWN && nfiles) sel = (sel + 1) % nfiles;
    if (in->pressed & (BTN_L | BTN_LEFT) && ndev) { cur_dev = (cur_dev + ndev - 1) % ndev; sel = 0; rescan(); }
    if (in->pressed & (BTN_R | BTN_RIGHT) && ndev) { cur_dev = (cur_dev + 1) % ndev; sel = 0; rescan(); }
    if (in->pressed & BTN_X) copy_to_other();
    if (in->pressed & BTN_Y) upload();
    if (in->pressed & BTN_A && nfiles) confirm_del = 1;
    if (in->pressed & BTN_START) { cloud_mode = 1; cloud_sel = 0; if (chatnet_online()) proto_send("VMULIST"); }
    return ACT_NONE;
}

void vmu_ui_draw(void) {
    theme_t *t = &g_theme;
    draw_topbar(tr(S_VMU_SAVES), NULL); draw_topbar_status();
    float dy = enter_dy(), lx = SAFE_X, ly = CONTENT_Y + dy, lw = 180, lh = CONTENT_H;
    panel(lx, ly, lw, lh);
    section_label(lx + 14, ly + 12, tr(S_SEC_MAPLE));
    if (!ndev) draw_text_wrap(&g_font_small, lx + 14, ly + 40, lw - 28, 15, t->text_dim, tr(S_VMU_NONE));
    for (int i = 0; i < ndev; i++) {
        float ry = ly + 40 + i * 30; int is = i == cur_dev;
        if (is) draw_rect_round(lx + 8, ry - 3, lw - 16, 26, 6, ALPHA(t->sel_bg, 230));
        char l[24]; snprintf(l, sizeof l, "VMU %c%d", 'A' + devs[i]->port, devs[i]->unit);
        draw_text(&g_font, lx + 16, ry, is ? t->sel_text : t->text, l);
    }
    if (ndev) { char fb[40]; snprintf(fb, sizeof fb, "%d %s", free_blocks, tr(S_FREE_BLOCKS)); draw_text(&g_font_small, lx + 14, ly + lh - 50, t->text_dim, fb); }
    draw_text_wrap(&g_font_small, lx + 14, ly + lh - 34, lw - 28, 14, ALPHA(t->text_dim, 180), tr(S_VMU_SD_NOTE));
    float rx = lx + lw + 12, rw = SCREEN_W - SAFE_X - rx;
    panel(rx, ly, rw, lh);
    section_label(rx + 14, ly + 12, cloud_mode ? tr(S_CLOUD_SAVES) : tr(S_VMU_SAVES));
    draw_set_clip((int)rx + 4, (int)ly + 34, (int)rw - 8, (int)lh - 44);
    if (!cloud_mode) {
        int first = sel > 9 ? sel - 9 : 0;
        for (int i = first; i < nfiles; i++) {
            float ry = ly + 38 + (i - first) * 22; int is = i == sel; char fn[13]; fname(&files[i], fn);
            if (is) draw_rect_round(rx + 10, ry - 2, rw - 20, 21, 5, ALPHA(t->sel_bg, 230));
            draw_text(&g_font, rx + 18, ry, is ? t->sel_text : t->text, fn);
            char sz[24]; snprintf(sz, sizeof sz, "%d %s", files[i].filesize, tr(S_BLOCKS));
            draw_text_right(&g_font_small, rx + rw - 90, ry + 3, is ? ALPHA(t->sel_text, 200) : t->text_dim, sz);
            draw_text_right(&g_font_small, rx + rw - 18, ry + 3, is ? ALPHA(t->sel_text, 200) : t->text_dim, files[i].filetype == 0xCC ? "GAME" : files[i].copyprotect ? "LOCK" : "DATA");
        }
        if (!nfiles && ndev) draw_text(&g_font_small, rx + 18, ly + 40, t->text_dim, tr(S_VMU_EMPTY));
    } else {
        if (!chatnet_online()) draw_text(&g_font_small, rx + 18, ly + 40, t->text_dim, tr(S_OFFLINE));
        else if (!ncloud) draw_text(&g_font_small, rx + 18, ly + 40, t->text_dim, tr(S_VMU_EMPTY));
        for (int i = 0; i < ncloud; i++) {
            float ry = ly + 38 + i * 22; int is = i == cloud_sel;
            if (is) draw_rect_round(rx + 10, ry - 2, rw - 20, 21, 5, ALPHA(t->sel_bg, 230));
            draw_text(&g_font, rx + 18, ry, is ? t->sel_text : t->text, cloud[i].fn);
            char sz[24]; snprintf(sz, sizeof sz, "%d %s", cloud[i].size / 512, tr(S_BLOCKS));
            draw_text_right(&g_font_small, rx + rw - 18, ry + 3, is ? ALPHA(t->sel_text, 200) : t->text_dim, sz);
        }
    }
    draw_clear_clip();
    draw_bottombar(tr(cloud_mode ? S_HINT_CLOUD : S_HINT_VMU));
    if (confirm_del) {
        dialog_box(tr(S_VMU_DELETE_Q), 360, 110);
        char fn[13]; fname(&files[sel], fn);
        draw_text_center(&g_font, SCREEN_W / 2, SCREEN_H / 2 - 4, t->text, fn);
        draw_hints(SCREEN_W / 2, SCREEN_H / 2 + 36, tr(S_HINT_DIALOG), 160);
    }
    (void)busy;
}
