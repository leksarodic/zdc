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
    static bool prev_r = false, prev_f = false, prev_g = false, prev_h = false;
    bool r = k[SDL_SCANCODE_R] != 0;
    if (r && !prev_r) ui_show_panel(0);
    prev_r = r;
    bool f = k[SDL_SCANCODE_F] != 0;
    if (f && !prev_f) ui_show_panel(1);
    prev_f = f;
    bool g = k[SDL_SCANCODE_G] != 0;
    if (g && !prev_g) ui_show_panel(2);
    prev_g = g;
    bool h = k[SDL_SCANCODE_H] != 0;
    if (h && !prev_h) ui_show_panel(-1);
    prev_h = h;
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
    const char *panel = NULL;
    const char *files_root = "media";
    bool show_call = false;
    bool light = false;
    bool dim = false;
    bool saver = false;
    bool show_vol = false;
    int vol_value = 70;
    const char *open_file = NULL;
    bool interrupt_demo = false;
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
        } else if (!strcmp(argv[i], "--panel") && i + 1 < argc) {
            panel = argv[++i];
        } else if (!strcmp(argv[i], "--files-root") && i + 1 < argc) {
            files_root = argv[++i];
        } else if (!strcmp(argv[i], "--call")) {
            show_call = true;
        } else if (!strcmp(argv[i], "--light")) {
            light = true;
        } else if (!strcmp(argv[i], "--dim")) {
            dim = true;
        } else if (!strcmp(argv[i], "--saver")) {
            saver = true;
        } else if (!strcmp(argv[i], "--interrupt")) {
            interrupt_demo = true;
        } else if (!strcmp(argv[i], "--file") && i + 1 < argc) {
            open_file = argv[++i];
        } else if (!strcmp(argv[i], "--volume") && i + 1 < argc) {
            show_vol = true;
            vol_value = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--screenshot out.ppm --frames N] [--state ...] [--panel radio|files|settings] [--call] [--files-root DIR] [--light] [--dim] [--saver] [--volume 0-100] [--file PATH] [--interrupt]\n", argv[0]);
            return 0;
        }
    }

    bt_model_t model;
    bt_sim_init(&model);
    bt_sim_set_state(&model, parse_state(state));

    lv_init();
    lv_display_t *disp = lv_sdl_window_create(800, 480);
    lv_sdl_window_set_title(disp, "CAR_482 7010B");
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_display_set_default(disp);

    ui_set_files_root(files_root);
    ui_init(&model);
    if (panel) {
        if (!strcmp(panel, "radio")) ui_show_panel(0);
        else if (!strcmp(panel, "files")) ui_show_panel(1);
        else if (!strcmp(panel, "settings")) ui_show_panel(2);
    }
    if (interrupt_demo) {
        bt_sim_set_state(&model, BT_PLAYING);
        bt_sim_set_state(&model, BT_INCOMING);
        ui_update();
    }
    if (show_call) ui_show_call();
    if (light) ui_set_theme(true);
    if (dim) ui_set_night_dim(true);
    if (saver) ui_show_screensaver();
    if (open_file) ui_open_file(open_file);
    if (show_vol) {
        ui_set_volume(vol_value);
        ui_show_volume();
    }

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

    printf("Test keys: 1-8 set state, Space primary, N next, P prev, M secondary/mute, R radio, F files, G settings, H home\n");
    for (;;) {
        handle_keys(&model);
        uint32_t wait = lv_timer_handler();
        usleep((useconds_t)((wait ? wait : 5u) * 1000u));
    }
}
