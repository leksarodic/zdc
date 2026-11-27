#include "ui.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"
#include "panels.h"
#include "call_panel.h"
#include "theme.h"

#define C_BG      theme_color(0x15171C)
#define C_CARD    theme_color(0x1E2128)
#define C_INNER   theme_color(0x262A33)
#define C_BTN     theme_color(0x2A2E37)
#define C_DOCK    theme_color(0x0D0E11)
#define C_TEXT    theme_color(0xF2F3F5)
#define C_MUTED   theme_color(0x8B9099)
#define C_SOFT    theme_color(0xC5C8CE)
#define C_CIRCLE  theme_color(0x23262E)
#define C_ACCENT  0xFE3211
#define C_GREEN   0x2DBE6C
#define C_RED     0xE3262F

static bt_model_t *M;

static uint32_t accent = C_ACCENT;
static bool clock_24h = true;
static time_t clock_base;
static time_t clock_base_at;
static bool clock_base_set = false;
static int screen_idx = -1;
typedef enum { SRC_BT = 0, SRC_RADIO = 1 } audio_source_t;
static audio_source_t active_src = SRC_BT;
static int radio_volume = 70;
static int bt_volume = 70;
static bool radio_muted = false;
static int radio_saved_volume = 0;
static int boot_volume = 70;     /* Settings slider: level used at power-on */
static int saver_timeout_sec = 300;
static bool night_dim = false;
static bool saver_enabled = true;

static lv_obj_t *vol_osd;
static lv_obj_t *vol_bar;
static lv_obj_t *vol_pct;
static lv_obj_t *vol_icon;
static lv_obj_t *vol_src_lbl;
static lv_timer_t *osd_timer;

static lv_obj_t *call_banner;
static lv_obj_t *call_banner_name;

static lv_obj_t *dim_overlay;
static lv_obj_t *saver;
static lv_obj_t *saver_time;
static lv_obj_t *saver_date;
static lv_timer_t *idle_timer;

static lv_obj_t *home;
static lv_obj_t *dock_circles[4];
static lv_obj_t *dock_labels[4];
static lv_obj_t *audio_source;

static lv_obj_t *player_panel;
static lv_obj_t *np_art;
static lv_obj_t *np_track;
static lv_obj_t *np_artist;
static lv_obj_t *np_time;
static lv_obj_t *np_bar;
static lv_obj_t *np_play_lbl;
static bool player_expanded = false;
static bt_state_t last_bt_state = BT_OFF;
static char call_target[48];

static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *top_bt_dot;

static lv_obj_t *phone_source;
static lv_obj_t *device_name;
static lv_obj_t *device_status;
static lv_obj_t *device_mac;
static lv_obj_t *disconnect_btn;
static lv_obj_t *disconnect_lbl;
static lv_obj_t *call_btn;

static lv_obj_t *bt_name_lbl;
static lv_obj_t *bt_arc;
static lv_obj_t *bt_circle;
static lv_obj_t *bt_title;
static lv_obj_t *bt_sub;
static lv_obj_t *primary_btn;
static lv_obj_t *primary_lbl;
static lv_obj_t *secondary_btn;
static lv_obj_t *secondary_lbl;

static lv_obj_t *art_circle;
static lv_obj_t *track_lbl;
static lv_obj_t *artist_lbl;
static lv_obj_t *time_pos_lbl;
static lv_obj_t *play_btn;
static lv_obj_t *play_lbl;
static lv_obj_t *prev_btn;
static lv_obj_t *next_btn;

static lv_timer_t *tick_timer;

static lv_color_t col(uint32_t hex) {
    return lv_color_hex(hex);
}

static void strip(lv_obj_t *o) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius) {
    lv_obj_t *o = lv_obj_create(parent);
    strip(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, col(bg), 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       const lv_font_t *font, uint32_t color, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    return l;
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h) {
    return box(parent, x, y, w, h, C_CARD, 16);
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                        uint32_t bg, uint32_t fg, const lv_font_t *font,
                        lv_event_cb_t cb, lv_obj_t **out_lbl) {
    lv_obj_t *b = lv_button_create(parent);
    strip(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, col(bg), 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(fg), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

static lv_obj_t *circle(lv_obj_t *parent, int x, int y, int d, uint32_t bg) {
    lv_obj_t *o = box(parent, x, y, d, d, bg, LV_RADIUS_CIRCLE);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    return o;
}

static lv_obj_t *symbol(lv_obj_t *parent, const char *sym, const lv_font_t *font, uint32_t color,
                        int x, int y, int w, int h) {
    return label(parent, sym, x, y, w, h, font, color, LV_TEXT_ALIGN_CENTER);
}

/* Symbol label centered on both axes (labels are content-sized, so this
 * centers the glyph box rather than a full-size text area). */
static lv_obj_t *icon_center(lv_obj_t *parent, const char *sym, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    lv_obj_center(l);
    return l;
}

static void clock_update(void);

static void pc_cb(lv_event_t *e) { (void)e; bt_sim_primary(M); ui_update(); }
static void sc_cb(lv_event_t *e) { (void)e; bt_sim_secondary(M); ui_update(); }
static void next_cb(lv_event_t *e) { (void)e; bt_sim_next(M); ui_update(); }
static void prev_cb(lv_event_t *e) { (void)e; bt_sim_prev(M); ui_update(); }
static void incoming_cb(lv_event_t *e) {
    (void)e;
    if (bt_sim_connected(M->state)) bt_sim_set_state(M, BT_INCOMING);
    ui_update();
}

static void tick_cb(lv_timer_t *t) {
    (void)t;
    bt_sim_tick(M, 1);
    ui_update();
    clock_update();
}

static time_t clock_now(void) {
    if (!clock_base_set) return time(NULL);
    return clock_base + (time(NULL) - clock_base_at);
}

static void clock_update(void) {
    time_t now = clock_now();
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[64];

    if (clock_24h) {
        strftime(buf, sizeof(buf), "%H:%M", &tm);
    } else {
        strftime(buf, sizeof(buf), "%I:%M %p", &tm);
        if (buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
    }
    lv_label_set_text(time_label, buf);

    strftime(buf, sizeof(buf), "%A, %d %B", &tm);
    lv_label_set_text(date_label, buf);

    if (saver_time) {
        char big[16];
        if (clock_24h) strftime(big, sizeof(big), "%H:%M", &tm);
        else {
            strftime(big, sizeof(big), "%I:%M", &tm);
            if (big[0] == '0') memmove(big, big + 1, strlen(big));
        }
        lv_label_set_text(saver_time, big);
        lv_label_set_text(saver_date, buf);
    }
}

static void set_text_if(lv_obj_t *l, const char *s) {
    const char *cur = lv_label_get_text(l);
    if (!cur || strcmp(cur, s) != 0) lv_label_set_text(l, s);
}

static const char *fmt_time(int sec, char *buf, size_t n) {
    if (sec < 0) sec = 0;
    snprintf(buf, n, "%d:%02d", sec / 60, sec % 60);
    return buf;
}

static void update_dock(int screen) {
    int active = screen < 0 ? 1 : screen == 0 ? 0 : screen == 1 ? 2 : 3;
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_border_width(dock_circles[i], i == active ? 2 : 0, 0);
        lv_obj_set_style_border_color(dock_circles[i], col(accent), 0);
        lv_obj_set_style_text_color(dock_labels[i], col(i == active ? C_TEXT : C_SOFT), 0);
    }
}

static void update_player_visibility(void);
static void update_call_banner(void);

static void show_screen(int idx) {
    screen_idx = idx;
    if (idx == 0) active_src = SRC_RADIO;
    else if (idx < 0) active_src = SRC_BT;
    call_panel_hide();
    if (idx < 0) {
        if (home) lv_obj_remove_flag(home, LV_OBJ_FLAG_HIDDEN);
        panels_hide();
    } else {
        if (home) lv_obj_add_flag(home, LV_OBJ_FLAG_HIDDEN);
        if (idx == 2) panels_set_now((long)clock_now());
        panels_show(idx);
    }
    update_dock(idx);
    update_player_visibility();
    update_call_banner();
}

static void on_home(void) { show_screen(-1); }

static void apply_accent(uint32_t c) {
    accent = c;
    if (phone_source) lv_obj_set_style_text_color(phone_source, col(accent), 0);
    if (bt_name_lbl) lv_obj_set_style_text_color(bt_name_lbl, col(accent), 0);
    if (audio_source) lv_obj_set_style_text_color(audio_source, col(accent), 0);
    if (play_btn) lv_obj_set_style_bg_color(play_btn, col(accent), 0);
    if (dock_circles[0]) update_dock(screen_idx);
    ui_update();
}

static void on_accent(uint32_t c) { apply_accent(c); }
static void on_clock(bool is24) { clock_24h = is24; clock_update(); }
static void on_set_time(long epoch) {
    clock_base = (time_t)epoch;
    clock_base_at = time(NULL);
    clock_base_set = true;
    clock_update();
}

/* ── volume overlay ─────────────────────────────────────────── */

static void osd_hide_cb(lv_timer_t *t) {
    if (vol_osd) lv_obj_add_flag(vol_osd, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(t);
}

static int *active_volume(void) {
    return active_src == SRC_RADIO ? &radio_volume : &bt_volume;
}

static void volume_render(void) {
    bool muted = active_src == SRC_RADIO && radio_muted;
    int v = *active_volume();
    if (vol_icon) lv_label_set_text(vol_icon, muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    if (vol_src_lbl) lv_label_set_text(vol_src_lbl, active_src == SRC_RADIO ? "RADIO" : "BLUETOOTH");
    if (vol_bar) lv_bar_set_value(vol_bar, muted ? 0 : v, LV_ANIM_OFF);
    if (vol_pct) {
        if (muted) lv_label_set_text(vol_pct, "MUTED");
        else lv_label_set_text_fmt(vol_pct, "%d%%", v);
    }
}

static void volume_show(void) {
    if (!vol_osd) return;
    volume_render();
    lv_obj_remove_flag(vol_osd, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(vol_osd);
    if (dim_overlay && night_dim) {
        lv_obj_move_foreground(dim_overlay);
        lv_obj_move_foreground(vol_osd);
    }
    if (osd_timer) {
        lv_timer_reset(osd_timer);
        lv_timer_resume(osd_timer);
    }
}

static void settings_load(void) {
    FILE *f = fopen("strada.settings", "r");
    if (!f) return;
    char key[32];
    int v;
    while (fscanf(f, " %31[^=]=%d", key, &v) == 2) {
        if (!strcmp(key, "volume") && v >= 0 && v <= 100) boot_volume = v;
        else if (!strcmp(key, "saver")) {
            if (v == 60 || v == 300 || v == 1800) saver_timeout_sec = v;
            else if (v > 0) saver_timeout_sec = 300; /* migrate old 15/30 s values */
        }
        int c;
        while ((c = fgetc(f)) != EOF && c != '\n') {}
    }
    fclose(f);
    radio_volume = boot_volume;
    bt_volume = boot_volume;
    radio_saved_volume = boot_volume;
}

static void settings_save(void) {
    FILE *f = fopen("strada.settings", "w");
    if (!f) return;
    fprintf(f, "volume=%d\nsaver=%d\n", boot_volume, saver_timeout_sec);
    fclose(f);
}

static void volume_apply(int v, bool show) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    *active_volume() = v;
    if (show) volume_show();
    else volume_render();
}

static void volume_open_cb(lv_event_t *e) { (void)e; show_screen(2); volume_show(); }
static void volume_nudge(int delta) {
    if (active_src == SRC_RADIO && radio_muted) {
        panels_set_radio_mute(false);
    }
    volume_apply(*active_volume() + delta, true);
}

static void volume_up_cb(lv_event_t *e) { (void)e; volume_nudge(5); }
static void volume_down_cb(lv_event_t *e) { (void)e; volume_nudge(-5); }
static void on_volume(int v) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    boot_volume = v;
    settings_save();
}

static void on_radio_start(void) {
    radio_volume = boot_volume;
    radio_saved_volume = boot_volume;
    active_src = SRC_RADIO;
    volume_render();
    volume_show();
}

static void on_radio_mute(bool muted) {
    if (muted) {
        if (!radio_muted) radio_saved_volume = radio_volume;
        radio_muted = true;
        radio_volume = 0;
    } else {
        radio_muted = false;
        radio_volume = radio_saved_volume > 0 ? radio_saved_volume : boot_volume;
    }
    volume_render();
    volume_show();
}

static void on_saver_timeout(int seconds) {
    if (seconds != 60 && seconds != 300 && seconds != 1800) return;
    saver_timeout_sec = seconds;
    settings_save();
    panels_set_saver_timeout(seconds);
}

static void call_banner_answer_cb(lv_event_t *e) {
    (void)e;
    if (M && M->state == BT_INCOMING) bt_sim_primary(M);
    show_screen(-1);
    ui_update();
}

static void call_banner_reject_cb(lv_event_t *e) {
    (void)e;
    if (M && M->state == BT_INCOMING) bt_sim_secondary(M);
    ui_update();
}

static void update_call_banner(void) {
    if (!call_banner) return;
    bool show = M && M->state == BT_INCOMING && screen_idx >= 0;
    if (show) {
        if (call_banner_name) lv_label_set_text(call_banner_name, BT_DEVICE_NAME "  /  +381 60 123 4567");
        lv_obj_remove_flag(call_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(call_banner);
    } else {
        lv_obj_add_flag(call_banner, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── theme and night mode ───────────────────────────────────── */

static void sync_panel_modes(void) {
    panels_set_modes(theme_is_light(), night_dim, saver_enabled);
}

static void on_theme(bool light) {
    theme_set_light(lv_screen_active(), light);
    if (vol_osd) lv_obj_set_style_bg_color(vol_osd, col(C_CARD), 0);
    if (vol_bar) lv_obj_set_style_bg_color(vol_bar, col(C_BTN), LV_PART_MAIN);
    if (dim_overlay) lv_obj_set_style_bg_color(dim_overlay, col(0x000000), 0);
    sync_panel_modes();
    ui_update();
}

static void on_night(bool on) {
    night_dim = on;
    if (!dim_overlay) return;
    if (on) {
        lv_obj_remove_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(dim_overlay);
        if (saver) lv_obj_move_foreground(saver);
    } else {
        lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_panel_modes();
}

static void saver_wake_cb(lv_event_t *e) {
    (void)e;
    if (saver) lv_obj_add_flag(saver, LV_OBJ_FLAG_HIDDEN);
}

static void saver_show_now(void) {
    if (!saver) return;
    clock_update();
    lv_obj_remove_flag(saver, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(saver);
}

static void idle_cb(lv_timer_t *t) {
    (void)t;
    if (!saver || !saver_enabled) return;
    if (!lv_obj_has_flag(saver, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_display_get_inactive_time(NULL) < (uint32_t)saver_timeout_sec * 1000u) return;
    saver_show_now();
}

static void on_saver(bool on) {
    saver_enabled = on;
    if (!on && saver) lv_obj_add_flag(saver, LV_OBJ_FLAG_HIDDEN);
    sync_panel_modes();
}

void ui_set_theme(bool light) { on_theme(light); }
void ui_set_night_dim(bool on) { on_night(on); }
void ui_set_screensaver(bool on) { on_saver(on); }
void ui_show_screensaver(void) { saver_show_now(); }
void ui_show_volume(void) { volume_show(); }
void ui_set_volume(int v) { volume_apply(v, false); }
void ui_volume_up(void) { volume_nudge(5); }
void ui_volume_down(void) { volume_nudge(-5); }
void ui_set_saver_timeout(int seconds) { on_saver_timeout(seconds); }
void ui_set_radio_mute(bool muted) { panels_set_radio_mute(muted); }
void ui_open_file(const char *path) { panels_open_file(path); }

static void dock_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx == 0) show_screen(0);
    else if (idx == 1) show_screen(-1);
    else if (idx == 2) show_screen(1);
    else show_screen(2);
}

static void update_player_visibility(void) {
    bool show = player_expanded && M && M->state == BT_PLAYING && screen_idx < 0 && !call_panel_visible();
    if (show) {
        lv_obj_remove_flag(player_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(player_panel);
    } else {
        lv_obj_add_flag(player_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void player_close_cb(lv_event_t *e) {
    (void)e;
    player_expanded = false;
    update_player_visibility();
}

static void art_open_cb(lv_event_t *e) {
    (void)e;
    if (M->state == BT_PLAYING) {
        player_expanded = true;
        update_player_visibility();
    }
}

static void on_outgoing_call(const char *number) {
    snprintf(call_target, sizeof(call_target), "%s", number ? number : "");
    bt_sim_set_state(M, BT_ACTIVE);
    M->call_seconds = 0;
    player_expanded = false;
    ui_update();
}

static void on_call_close(void) {
    show_screen(-1);
}

static void call_open_cb(lv_event_t *e) {
    (void)e;
    if (bt_sim_connected(M->state)) call_panel_show();
}

static void update_player(void) {
    if (!player_panel) return;
    const bt_track_t *t = bt_sim_track(M);
    char pos[32], dur[32], line[64];
    set_text_if(np_track, t->title);
    set_text_if(np_artist, t->artist);
    fmt_time(M->position, pos, sizeof(pos));
    fmt_time(t->duration, dur, sizeof(dur));
    snprintf(line, sizeof(line), "%s / %s", pos, dur);
    set_text_if(np_time, line);
    lv_label_set_text(np_play_lbl, (M->state == BT_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_bar_set_range(np_bar, 0, t->duration);
    lv_bar_set_value(np_bar, M->position, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_art, col(accent), 0);
}

static void update_phone(void) {
    bool connected = bt_sim_connected(M->state);
    set_text_if(phone_source, connected ? "A2DP" : "-");
    set_text_if(device_name, connected ? BT_DEVICE_NAME : "No device");
    set_text_if(device_mac, connected ? BT_DEVICE_MAC : "-");
    set_text_if(disconnect_lbl, connected ? "Disconnect" : "Pair from phone");

    const char *status = "Bluetooth off";
    uint32_t color = C_MUTED;
    switch (M->state) {
    case BT_OFF: status = "Bluetooth off"; break;
    case BT_READY: status = "Pair from your phone"; break;
    case BT_PAIRING: status = "Pairing..."; break;
    case BT_CONNECTED: status = "Connected"; color = C_GREEN; break;
    case BT_PLAYING: status = "Connected"; color = C_GREEN; break;
    case BT_INCOMING: status = "Incoming call"; color = C_GREEN; break;
    case BT_ACTIVE: status = "In call"; color = C_GREEN; break;
    case BT_ERROR: status = "Connection error"; color = C_ACCENT; break;
    }
    set_text_if(device_status, status);
    lv_obj_set_style_text_color(device_status, col(color), 0);

    if (connected) {
        lv_obj_remove_state(call_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(call_btn, LV_STATE_DISABLED);
    }
}

static void update_bt(void) {
    bool connected = bt_sim_connected(M->state);
    int value = 0;
    uint32_t arc_color = accent;
    uint32_t core = accent;
    const char *title = "";
    const char *sub = "";
    const char *primary = "";
    const char *secondary = "";
    uint32_t primary_bg = accent;
    char callbuf[32];

    switch (M->state) {
    case BT_OFF:
        value = 0; core = 0x3A3E48;
        title = "Bluetooth off";
        sub = "Tap to power on";
        primary = "Power on";
        secondary = "Settings";
        break;
    case BT_READY:
        value = 18; core = 0x3A3E48;
        title = "Pair from your phone";
        sub = "PIN 0000 / Open Bluetooth on your phone.";
        primary = "Start pairing";
        secondary = "Settings";
        break;
    case BT_PAIRING:
        value = 55; core = 0x4A4F5A;
        title = "Waiting for phone";
        sub = "Select " BT_LOCAL_NAME " on your phone";
        primary = "Cancel";
        secondary = "";
        break;
    case BT_CONNECTED:
        value = 78;
        title = "Connected";
        sub = BT_DEVICE_NAME " / A2DP / AVRCP";
        primary = "Play";
        secondary = "Disconnect";
        break;
    case BT_PLAYING:
        value = 78;
        title = "Playing";
        sub = BT_DEVICE_NAME " / A2DP / AVRCP";
        primary = "Pause";
        secondary = "Disconnect";
        break;
    case BT_INCOMING:
        value = 90; arc_color = C_GREEN; core = C_GREEN;
        title = "Incoming call";
        sub = BT_DEVICE_NAME " / +381 60 123 4567";
        primary = "Answer";
        secondary = "Reject";
        primary_bg = C_GREEN;
        break;
    case BT_ACTIVE:
        value = 78; arc_color = C_GREEN; core = C_GREEN;
        title = "In call";
        {
            char tbuf[16];
            fmt_time(M->call_seconds, tbuf, sizeof(tbuf));
            if (call_target[0]) snprintf(callbuf, sizeof(callbuf), "%s / %s", tbuf, call_target);
            else snprintf(callbuf, sizeof(callbuf), "%s / car audio", tbuf);
            sub = callbuf;
        }
        primary = "Hang up";
        secondary = M->muted ? "Unmute" : "Mute";
        primary_bg = C_RED;
        break;
    case BT_ERROR:
        value = 25; core = 0x4A4F5A;
        title = "Connection failed";
        sub = "Check your phone and try again";
        primary = "Retry";
        secondary = "Settings";
        break;
    }

    lv_arc_set_value(bt_arc, value);
    lv_obj_set_style_arc_color(bt_arc, col(arc_color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bt_circle, col(core), 0);

    set_text_if(bt_name_lbl, BT_LOCAL_NAME);
    set_text_if(bt_title, title);
    set_text_if(bt_sub, sub);
    set_text_if(primary_lbl, primary);
    set_text_if(secondary_lbl, secondary);
    lv_obj_set_style_bg_color(primary_btn, col(primary_bg), 0);

    if (secondary[0]) {
        lv_obj_remove_flag(secondary_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(secondary_btn, LV_OBJ_FLAG_HIDDEN);
    }

    uint32_t dot = C_MUTED;
    if (connected) dot = C_GREEN;
    else if (M->state == BT_PAIRING || M->state == BT_ERROR) dot = accent;
    lv_obj_set_style_bg_color(top_bt_dot, col(dot), 0);

    lv_obj_set_style_bg_color(art_circle, col(connected ? accent : 0x3A3E48), 0);
}

static void update_audio(void) {
    bool connected = bt_sim_connected(M->state);
    const bt_track_t *t = bt_sim_track(M);
    char pos[32], dur[32], line[96];

    bool interrupted = bt_sim_music_interrupted(M);
    set_text_if(track_lbl, connected ? t->title : "Nothing playing");
    set_text_if(artist_lbl, interrupted ? "Paused for call" : (connected ? t->artist : "Radio, Bluetooth or USB"));
    fmt_time(M->position, pos, sizeof(pos));
    fmt_time(t->duration, dur, sizeof(dur));
    snprintf(line, sizeof(line), "%s / %s", pos, dur);
    set_text_if(time_pos_lbl, connected ? line : "-");

    lv_label_set_text(play_lbl, (M->state == BT_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (connected) {
        lv_obj_remove_state(play_btn, LV_STATE_DISABLED);
        lv_obj_remove_state(prev_btn, LV_STATE_DISABLED);
        lv_obj_remove_state(next_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(play_btn, LV_STATE_DISABLED);
        lv_obj_add_state(prev_btn, LV_STATE_DISABLED);
        lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    }
}

void ui_update(void) {
    if (!M) return;

    if (M->state == BT_PLAYING && last_bt_state != BT_PLAYING) player_expanded = true;
    if (M->state != BT_PLAYING) player_expanded = false;
    if (M->state != BT_ACTIVE) call_target[0] = '\0';

    update_phone();
    update_bt();
    update_audio();
    update_player();
    update_player_visibility();
    update_call_banner();
    last_bt_state = M->state;
}

static void build_player(lv_obj_t *scr) {
    player_panel = card(scr, 12, 50, 776, 318);
    lv_obj_add_flag(player_panel, LV_OBJ_FLAG_HIDDEN);

    label(player_panel, "NOW PLAYING", 18, 12, 200, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(player_panel, "BLUETOOTH", 520, 12, 200, 18, &lv_font_montserrat_12, accent, LV_TEXT_ALIGN_RIGHT);
    button(player_panel, LV_SYMBOL_DOWN, 726, 8, 32, 32, C_INNER, 0xFFFFFF, &lv_font_montserrat_12, player_close_cb, NULL);

    np_art = circle(player_panel, 40, 44, 200, accent);
    icon_center(np_art, LV_SYMBOL_AUDIO, &lv_font_montserrat_40, 0xFFFFFF);

    np_track = label(player_panel, "Test Song", 280, 52, 460, 38, &lv_font_montserrat_26, C_TEXT, LV_TEXT_ALIGN_LEFT);
    np_artist = label(player_panel, "Demo Artist", 280, 96, 460, 22, &lv_font_montserrat_16, C_MUTED, LV_TEXT_ALIGN_LEFT);
    np_time = label(player_panel, "1:08 / 3:35", 280, 124, 460, 16, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);

    np_bar = lv_bar_create(player_panel);
    lv_obj_set_pos(np_bar, 280, 150);
    lv_obj_set_size(np_bar, 440, 8);
    lv_bar_set_range(np_bar, 0, 100);
    lv_bar_set_value(np_bar, 68, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(np_bar, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(np_bar, col(accent), LV_PART_INDICATOR);
    lv_obj_set_style_radius(np_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(np_bar, 4, LV_PART_INDICATOR);

    lv_obj_t *prev = button(player_panel, LV_SYMBOL_PREV, 280, 190, 54, 54, C_BTN, 0xFFFFFF,
                            &lv_font_montserrat_20, prev_cb, NULL);
    lv_obj_set_style_radius(prev, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *play = button(player_panel, LV_SYMBOL_PLAY, 350, 182, 70, 70, accent, 0xFFFFFF,
                            &lv_font_montserrat_28, pc_cb, &np_play_lbl);
    lv_obj_set_style_radius(play, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *next = button(player_panel, LV_SYMBOL_NEXT, 436, 190, 54, 54, C_BTN, 0xFFFFFF,
                            &lv_font_montserrat_20, next_cb, NULL);
    lv_obj_set_style_radius(next, LV_RADIUS_CIRCLE, 0);

    label(player_panel, "Tap the album art on the home card to reopen this player", 280, 270, 460, 14,
          &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
}

static void build_volume_osd(lv_obj_t *scr) {
    vol_osd = box(scr, 220, 274, 360, 70, C_CARD, 16);
    lv_obj_set_style_shadow_width(vol_osd, 26, 0);
    lv_obj_set_style_shadow_opa(vol_osd, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(vol_osd, col(0x000000), 0);
    lv_obj_add_flag(vol_osd, LV_OBJ_FLAG_HIDDEN);

    button(vol_osd, "-", 14, 18, 40, 34, C_INNER, C_TEXT, &lv_font_montserrat_18,
           volume_down_cb, NULL);
    vol_icon = symbol(vol_osd, LV_SYMBOL_VOLUME_MAX, &lv_font_montserrat_18, C_SOFT, 62, 22, 32, 26);
    vol_src_lbl = label(vol_osd, "BLUETOOTH", 104, 7, 166, 12,
                        &lv_font_montserrat_10, accent, LV_TEXT_ALIGN_LEFT);

    vol_bar = lv_bar_create(vol_osd);
    lv_obj_set_pos(vol_bar, 104, 30);
    lv_obj_set_size(vol_bar, 166, 12);
    lv_bar_set_range(vol_bar, 0, 100);
    lv_bar_set_value(vol_bar, bt_volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vol_bar, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(vol_bar, col(accent), LV_PART_INDICATOR);
    lv_obj_set_style_radius(vol_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(vol_bar, 6, LV_PART_INDICATOR);

    vol_pct = label(vol_osd, "70%", 262, 22, 50, 26, &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_RIGHT);
    button(vol_osd, "+", 314, 18, 34, 34, C_INNER, C_TEXT, &lv_font_montserrat_18,
           volume_up_cb, NULL);

    osd_timer = lv_timer_create(osd_hide_cb, 1800, NULL);
    lv_timer_pause(osd_timer);
}

static void build_call_banner(lv_obj_t *scr) {
    call_banner = box(scr, 190, 60, 420, 96, C_CARD, 16);
    lv_obj_add_flag(call_banner, LV_OBJ_FLAG_HIDDEN);
    label(call_banner, "INCOMING CALL", 16, 10, 200, 14,
          &lv_font_montserrat_10, accent, LV_TEXT_ALIGN_LEFT);
    call_banner_name = label(call_banner, BT_DEVICE_NAME, 16, 28, 388, 20,
                             &lv_font_montserrat_14, C_TEXT, LV_TEXT_ALIGN_LEFT);
    button(call_banner, "Answer", 16, 56, 180, 30, C_GREEN, C_TEXT,
           &lv_font_montserrat_12, call_banner_answer_cb, NULL);
    button(call_banner, "Reject", 224, 56, 180, 30, C_INNER, C_TEXT,
           &lv_font_montserrat_12, call_banner_reject_cb, NULL);
}

static void build_overlays(lv_obj_t *scr) {
    dim_overlay = box(scr, 0, 0, 800, 480, 0x000000, 0);
    lv_obj_set_style_bg_opa(dim_overlay, LV_OPA_50, 0);
    lv_obj_remove_flag(dim_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);

    saver = box(scr, 0, 0, 800, 480, 0x07080A, 0);
    lv_obj_add_flag(saver, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(saver, saver_wake_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(saver, LV_OBJ_FLAG_HIDDEN);

    label(saver, BT_LOCAL_NAME, 0, 78, 800, 18, &lv_font_montserrat_12, 0xFE3211, LV_TEXT_ALIGN_CENTER);
    saver_time = label(saver, "00:00", 0, 158, 800, 78, &lv_font_montserrat_40, 0xF7F8FA,
                       LV_TEXT_ALIGN_CENTER);
    saver_date = label(saver, "", 0, 248, 800, 20, &lv_font_montserrat_14, 0x9AA0A9,
                       LV_TEXT_ALIGN_CENTER);
    label(saver, "Tap anywhere to wake", 0, 330, 800, 16, &lv_font_montserrat_10, 0x9AA0A9,
          LV_TEXT_ALIGN_CENTER);
}

void ui_init(bt_model_t *model) {
    M = model;
    setlocale(LC_TIME, "C");
    settings_load();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, col(C_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    home = lv_obj_create(scr);
    strip(home);
    lv_obj_set_pos(home, 0, 0);
    lv_obj_set_size(home, 800, 480);
    lv_obj_set_style_bg_opa(home, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(home, LV_OBJ_FLAG_CLICKABLE);

    /* ── top bar (no weather) ───────────────────────────────── */
    time_label = label(scr, "00:00", 16, 4, 92, 34, &lv_font_montserrat_26, C_TEXT, LV_TEXT_ALIGN_LEFT);
    date_label = label(scr, "", 112, 12, 260, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);

    symbol(scr, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_18, C_SOFT, 674, 10, 22, 22);
    top_bt_dot = circle(scr, 692, 25, 6, C_GREEN);

    lv_obj_t *vol_btn = circle(scr, 744, 8, 32, C_CIRCLE);
    icon_center(vol_btn, LV_SYMBOL_VOLUME_MAX, &lv_font_montserrat_16, C_TEXT);
    lv_obj_add_event_cb(vol_btn, volume_open_cb, LV_EVENT_CLICKED, NULL);

    /* ── phone card ─────────────────────────────────────────── */
    lv_obj_t *phone = card(home, 12, 50, 234, 318);
    label(phone, "PHONE", 14, 12, 100, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    phone_source = label(phone, "A2DP", 144, 12, 76, 14, &lv_font_montserrat_10, accent, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *dev_ic = circle(phone, 14, 34, 46, C_CIRCLE);
    icon_center(dev_ic, LV_SYMBOL_CALL, &lv_font_montserrat_24, C_SOFT);

    device_name = label(phone, BT_DEVICE_NAME, 70, 36, 150, 20, &lv_font_montserrat_16, C_TEXT, LV_TEXT_ALIGN_LEFT);
    device_status = label(phone, "Connected", 70, 58, 150, 14, &lv_font_montserrat_12, C_GREEN, LV_TEXT_ALIGN_LEFT);
    device_mac = label(phone, BT_DEVICE_MAC, 70, 76, 150, 12, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);

    label(phone, "Signal 5/5", 14, 100, 100, 14, &lv_font_montserrat_10, C_SOFT, LV_TEXT_ALIGN_LEFT);
    label(phone, "Battery 82%", 120, 100, 100, 14, &lv_font_montserrat_10, C_SOFT, LV_TEXT_ALIGN_LEFT);

    box(phone, 14, 120, 206, 1, C_BTN, 0);
    label(phone, "Phone connects to the car", 14, 136, 206, 14, &lv_font_montserrat_12,
          C_SOFT, LV_TEXT_ALIGN_LEFT);
    label(phone, "Open Bluetooth on your phone", 14, 156, 206, 12, &lv_font_montserrat_10,
          C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(phone, "and select " BT_LOCAL_NAME ".", 14, 174, 206, 12, &lv_font_montserrat_10,
          C_MUTED, LV_TEXT_ALIGN_LEFT);

    disconnect_btn = button(phone, "Disconnect", 14, 268, 130, 36, C_INNER, C_TEXT,
                            &lv_font_montserrat_12, sc_cb, &disconnect_lbl);
    call_btn = button(phone, "Call", 150, 268, 70, 36, C_INNER, C_TEXT,
                      &lv_font_montserrat_12, call_open_cb, NULL);

    /* ── bluetooth card ─────────────────────────────────────── */
    lv_obj_t *bt = card(home, 256, 50, 328, 318);
    label(bt, "BLUETOOTH", 14, 12, 120, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    bt_name_lbl = label(bt, BT_LOCAL_NAME, 220, 12, 94, 14, &lv_font_montserrat_10, accent, LV_TEXT_ALIGN_RIGHT);

    bt_arc = lv_arc_create(bt);
    strip(bt_arc);
    lv_obj_set_pos(bt_arc, 89, 32);
    lv_obj_set_size(bt_arc, 150, 150);
    lv_obj_set_style_bg_opa(bt_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_width(bt_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(bt_arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(bt_arc, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_arc_color(bt_arc, col(accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(bt_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(bt_arc, true, LV_PART_INDICATOR);
    lv_arc_set_rotation(bt_arc, 270);
    lv_arc_set_bg_angles(bt_arc, 0, 360);
    lv_arc_set_range(bt_arc, 0, 100);
    lv_arc_set_value(bt_arc, 78);
    lv_obj_remove_flag(bt_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(bt_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(bt_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(bt_arc, 0, LV_PART_KNOB);

    bt_circle = circle(bt, 99, 42, 130, accent);
    icon_center(bt_circle, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_40, 0xFFFFFF);

    bt_title = label(bt, "Connected", 14, 186, 300, 28, &lv_font_montserrat_22, C_TEXT, LV_TEXT_ALIGN_CENTER);
    bt_sub = label(bt, "", 14, 212, 300, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_CENTER);

    primary_btn = button(bt, "Play", 54, 262, 130, 36, accent, 0xFFFFFF,
                         &lv_font_montserrat_12, pc_cb, &primary_lbl);
    secondary_btn = button(bt, "Disconnect", 190, 262, 110, 36, C_INNER, C_TEXT,
                           &lv_font_montserrat_12, sc_cb, &secondary_lbl);


    /* ── audio card ─────────────────────────────────────────── */
    lv_obj_t *audio = card(home, 594, 50, 194, 318);
    label(audio, "AUDIO", 14, 12, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    audio_source = label(audio, "BLUETOOTH", 90, 12, 90, 14, &lv_font_montserrat_10, accent, LV_TEXT_ALIGN_RIGHT);

    art_circle = circle(audio, 33, 32, 128, accent);
    icon_center(art_circle, LV_SYMBOL_AUDIO, &lv_font_montserrat_40, 0xFFFFFF);
    lv_obj_add_event_cb(art_circle, art_open_cb, LV_EVENT_CLICKED, NULL);

    track_lbl = label(audio, "Test Song", 8, 170, 178, 20, &lv_font_montserrat_14, C_TEXT, LV_TEXT_ALIGN_CENTER);
    artist_lbl = label(audio, "Demo Artist", 8, 190, 178, 16, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_CENTER);
    time_pos_lbl = label(audio, "1:08 / 3:35", 8, 206, 178, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_CENTER);

    prev_btn = button(audio, LV_SYMBOL_PREV, 30, 242, 42, 42, C_BTN, 0xFFFFFF,
                      &lv_font_montserrat_18, prev_cb, NULL);
    lv_obj_set_style_radius(prev_btn, LV_RADIUS_CIRCLE, 0);
    play_btn = button(audio, LV_SYMBOL_PLAY, 70, 236, 54, 54, accent, 0xFFFFFF,
                      &lv_font_montserrat_24, pc_cb, &play_lbl);
    lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, 0);
    next_btn = button(audio, LV_SYMBOL_NEXT, 130, 242, 42, 42, C_BTN, 0xFFFFFF,
                      &lv_font_montserrat_18, next_cb, NULL);
    lv_obj_set_style_radius(next_btn, LV_RADIUS_CIRCLE, 0);

    /* ── dock: Radio, Bluetooth, Files, Settings ────────────── */
    lv_obj_t *dock = box(scr, 0, 392, 800, 88, C_DOCK, 0);
    const char *dock_icons[] = { LV_SYMBOL_AUDIO, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_DIRECTORY, LV_SYMBOL_SETTINGS };
    const char *dock_names[] = { "Radio", "Bluetooth", "Files", "Settings" };
    for (int i = 0; i < 4; i++) {
        int slot = 180 + i * 110;
        lv_obj_t *c = circle(dock, slot + 35, 6, 40, C_CIRCLE);
        dock_circles[i] = c;
        icon_center(c, dock_icons[i], &lv_font_montserrat_20, 0xFFFFFF);
        dock_labels[i] = label(dock, dock_names[i], slot + 5, 50, 100, 14, &lv_font_montserrat_10,
                               i == 1 ? C_TEXT : C_SOFT, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_event_cb(c, dock_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    panels_init(scr, on_home, on_accent, on_clock, on_set_time, on_volume,
                on_theme, on_night, on_saver, on_radio_start, on_radio_mute,
                on_saver_timeout);
    build_player(scr);
    call_panel_init(scr, on_call_close, on_outgoing_call);
    build_volume_osd(scr);
    build_call_banner(scr);
    radio_volume = boot_volume;
    bt_volume = boot_volume;
    radio_saved_volume = boot_volume;
    volume_apply(bt_volume, false);
    panels_set_boot_volume(boot_volume);
    panels_set_saver_timeout(saver_timeout_sec);
    build_overlays(scr);
    show_screen(-1);

    ui_update();
    clock_update();

    tick_timer = lv_timer_create(tick_cb, 1000, NULL);
    idle_timer = lv_timer_create(idle_cb, 1000, NULL);
    (void)tick_timer;
    (void)idle_timer;
}

void ui_set_files_root(const char *root) {
    panels_set_files_root(root);
}

void ui_show_panel(int idx) {
    show_screen(idx);
}

void ui_show_call(void) {
    if (M && bt_sim_connected(M->state)) call_panel_show();
}

#ifdef STRADA_USE_SDL
void ui_screenshot(const char *path) {
    lv_refr_now(NULL);
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    if (!buf) return;

    FILE *f = fopen(path, "wb");
    if (!f) {
        lv_draw_buf_destroy(buf);
        return;
    }
    fprintf(f, "P6\n%u %u\n255\n", (unsigned)buf->header.w, (unsigned)buf->header.h);
    for (uint32_t y = 0; y < buf->header.h; y++) {
        const uint8_t *row = buf->data + (size_t)y * buf->header.stride;
        for (uint32_t x = 0; x < buf->header.w; x++) {
            uint8_t rgb[3] = { row[x * 3 + 2], row[x * 3 + 1], row[x * 3 + 0] };
            fwrite(rgb, 1, sizeof(rgb), f);
        }
    }
    fclose(f);
    lv_draw_buf_destroy(buf);
}
#endif
