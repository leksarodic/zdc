#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "bt_sim.h"
#include "ui.h"

static void handle_keys(bt_model_t *m) {
    SDL_PumpEvents();
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    static bool prev[8] = {0};
    static bool prev_space = false, prev_n = false, prev_p = false, prev_m = false;
    const SDL_Scancode codes[8] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8
    };
    const bt_state_t states[8] = {
        BT_OFF, BT_READY, BT_PAIRING, BT_CONNECTED,
        BT_PLAYING, BT_INCOMING, BT_ACTIVE, BT_ERROR
    };
    for (int i = 0; i < 8; i++) {
        bool now = k[codes[i]] != 0;
        if (now && !prev[i]) {
            bt_sim_set_state(m, states[i]);
            ui_update();
        }
        prev[i] = now;
    }
    bool space = k[SDL_SCANCODE_SPACE] != 0;
    if (space && !prev_space) { bt_sim_primary(m); ui_update(); }
    prev_space = space;
    bool n = k[SDL_SCANCODE_N] != 0;
    if (n && !prev_n) { bt_sim_next(m); ui_update(); }
    prev_n = n;
    bool p = k[SDL_SCANCODE_P] != 0;
    if (p && !prev_p) { bt_sim_prev(m); ui_update(); }
    prev_p = p;
    bool mm = k[SDL_SCANCODE_M] != 0;
    if (mm && !prev_m) { bt_sim_secondary(m); ui_update(); }
    prev_m = mm;
}

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

    printf("Test keys: 1-8 set state, Space primary, N next, P prev, M secondary/mute\n");
    for (;;) {
        handle_keys(&model);
        uint32_t wait = lv_timer_handler();
        usleep((useconds_t)((wait ? wait : 5u) * 1000u));
    }
}
