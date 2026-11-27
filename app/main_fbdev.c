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

static bool find_storage_root(char *out, size_t n) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return false;
    char dev[256], mnt[512], type[64];
    while (fscanf(f, "%255s %511s %63s %*[^\n]\n", dev, mnt, type) == 3) {
        if (!strstr(mnt, "/media/") && !strstr(mnt, "/mnt/") && !strstr(mnt, "/run/media/")) continue;
        if (!strcmp(type, "vfat") || !strcmp(type, "exfat") || !strcmp(type, "ntfs") ||
            !strcmp(type, "fuseblk") || !strcmp(type, "msdos")) {
            snprintf(out, n, "%s", mnt);
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}

int main(int argc, char **argv) {
    const char *fb = "/dev/fb0";
    const char *touch = "/dev/input/event0";
    const char *buttons = NULL;
    char storage_path[512];
    const char *files_root = find_storage_root(storage_path, sizeof(storage_path)) ? storage_path : "/nonexistent";

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
    lv_linux_fbdev_set_force_refresh(disp, true);
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

    ui_set_files_root(files_root);
    ui_init(&model);
    printf("CAR_482 7010B UI on %s (touch %s, buttons %s)\n", fb, touch, buttons ? buttons : "none");

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
