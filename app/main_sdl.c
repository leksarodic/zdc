#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "bt_sim.h"
#include "ui.h"

static bt_state_t parse_state(const char *s) {
    if (!strcmp(s, "off")) return BT_OFF;
    if (!strcmp(s, "ready")) return BT_READY;
    if (!strcmp(s, "pairing")) return BT_PAIRING;
    if (!strcmp(s, "connected")) return BT_CONNECTED;
    if (!strcmp(s, "playing")) return BT_PLAYING;
    if (!strcmp(s, "incoming")) return BT_INCOMING;
    if (!strcmp(s, "active")) return BT_ACTIVE;
    if (!strcmp(s, "error")) return BT_ERROR;
    return BT_CONNECTED;
}

int main(int argc, char **argv) {
    const char *shot = NULL;
    const char *state = "connected";
    int frames = 0;
    bool interactive = true;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) {
            shot = argv[++i];
            interactive = false;
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--state") && i + 1 < argc) {
            state = argv[++i];
        } else if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--screenshot out.ppm --frames N] [--state off|ready|pairing|connected|playing|incoming|active|error]\n", argv[0]);
            return 0;
        }
    }

    bt_model_t model;
    bt_sim_init(&model);
    bt_sim_set_state(&model, parse_state(state));

    lv_init();
    lv_display_t *disp = lv_sdl_window_create(800, 480);
    lv_sdl_window_set_title(disp, "Strada 7010B");
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_display_set_default(disp);

    ui_init(&model);

    if (shot) {
        if (frames <= 0) frames = 90;
        for (int i = 0; i < frames; i++) {
            uint32_t wait = lv_timer_handler();
            usleep((useconds_t)((wait ? wait : 5u) * 1000u));
        }
        lv_refr_now(disp);
        ui_screenshot(shot);
        printf("screenshot: %s (state=%s)\n", shot, bt_sim_state_name(model.state));
        return 0;
    }

    if (!interactive) return 0;

    for (;;) {
        uint32_t wait = lv_timer_handler();
        usleep((useconds_t)((wait ? wait : 5u) * 1000u));
    }
}
