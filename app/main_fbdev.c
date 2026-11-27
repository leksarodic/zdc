#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#include "src/drivers/evdev/lv_evdev.h"

#include "bt_sim.h"
#include "ui.h"

static uint32_t tick_get_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static int find_storage_roots(char roots[][512], char labels[][16], int max) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;
    int count = 0;
    char dev[256], mnt[512], type[64];
    while (count < max && fscanf(f, "%255s %511s %63s %*[^\n]\n", dev, mnt, type) == 3) {
        if (!strstr(mnt, "/media/") && !strstr(mnt, "/mnt/") && !strstr(mnt, "/run/media/")) continue;
        if (strcmp(type, "vfat") && strcmp(type, "exfat") && strcmp(type, "ntfs") &&
            strcmp(type, "fuseblk") && strcmp(type, "msdos")) continue;
        bool dup = false;
        for (int i = 0; i < count; i++) if (!strcmp(roots[i], mnt)) dup = true;
        if (dup) continue;
        snprintf(roots[count], 512, "%s", mnt);
        const char *label = strstr(dev, "mmcblk") ? "SD" : (strstr(dev, "sd") ? "USB" : "STORAGE");
        snprintf(labels[count], 16, "%s", label);
        count++;
    }
    fclose(f);
    return count;
}

int main(int argc, char **argv) {
    const char *fb = "/dev/fb0";
    const char *touch = "/dev/input/event0";
    const char *buttons = NULL;
    const char *files_root = NULL;
    char storage_roots[4][512];
    char storage_labels[4][16];
    int storage_count = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fb") && i + 1 < argc) fb = argv[++i];
        else if (!strcmp(argv[i], "--touch") && i + 1 < argc) touch = argv[++i];
        else if (!strcmp(argv[i], "--buttons") && i + 1 < argc) buttons = argv[++i];
        else if (!strcmp(argv[i], "--files-root") && i + 1 < argc) files_root = argv[++i];
        else if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--fb /dev/fb0] [--touch /dev/input/event0] [--buttons /dev/input/event1] [--files-root /media]\n", argv[0]);
            return 0;
        }
    }

    bt_model_t model;
    bt_sim_init(&model);

    lv_init();
    lv_tick_set_cb(tick_get_cb);

    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "fbdev: cannot create display\n");
        return 1;
    }
    lv_linux_fbdev_set_file(disp, fb);
    lv_linux_fbdev_set_force_refresh(disp, false);
    lv_display_set_default(disp);

    if (touch && touch[0]) {
        lv_indev_t *indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch);
        if (!indev) fprintf(stderr, "evdev: cannot open %s (UI still runs)\n", touch);
    }

    int btn_fd = -1;
    if (buttons && buttons[0]) {
        btn_fd = open(buttons, O_RDONLY | O_NONBLOCK);
        if (btn_fd < 0) fprintf(stderr, "evdev: cannot open buttons %s (volume keys disabled)\n", buttons);
    }

    if (files_root) {
        snprintf(storage_roots[0], sizeof(storage_roots[0]), "%s", files_root);
        snprintf(storage_labels[0], sizeof(storage_labels[0]), "STORAGE");
        storage_count = 1;
    } else {
        storage_count = find_storage_roots(storage_roots, storage_labels, 4);
        if (storage_count == 0) {
            snprintf(storage_roots[0], sizeof(storage_roots[0]), "/nonexistent");
            snprintf(storage_labels[0], sizeof(storage_labels[0]), "STORAGE");
            storage_count = 1;
        }
    }
    const char *root_ptrs[4];
    const char *label_ptrs[4];
    for (int i = 0; i < storage_count; i++) {
        root_ptrs[i] = storage_roots[i];
        label_ptrs[i] = storage_labels[i];
    }
    ui_set_storage_roots(root_ptrs, label_ptrs, storage_count);
    ui_init(&model);
    printf("CAR_482 7010B UI on %s (touch %s, buttons %s, storage roots %d)\n",
           fb, touch, buttons ? buttons : "none", storage_count);

    for (;;) {
        if (btn_fd >= 0) {
            struct input_event ev;
            while (read(btn_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type != EV_KEY || ev.value != 1) continue;
                if (ev.code == KEY_VOLUMEUP) ui_volume_up();
                else if (ev.code == KEY_VOLUMEDOWN) ui_volume_down();
            }
        }
        uint32_t wait = lv_timer_handler();
        usleep((useconds_t)((wait ? wait : 5u) * 1000u));
    }
}
