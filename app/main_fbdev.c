#include <stdbool.h>
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

int main(int argc, char **argv) {
    const char *fb = "/dev/fb0";
    const char *touch = "/dev/input/event0";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fb") && i + 1 < argc) fb = argv[++i];
        else if (!strcmp(argv[i], "--touch") && i + 1 < argc) touch = argv[++i];
        else if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--fb /dev/fb0] [--touch /dev/input/event0]\n", argv[0]);
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

    ui_set_files_root("/media");
    ui_init(&model);
    printf("Strada 7010B UI on %s (touch %s)\n", fb, touch);

    for (;;) {
        uint32_t wait = lv_timer_handler();
        usleep((useconds_t)((wait ? wait : 5u) * 1000u));
    }
}
