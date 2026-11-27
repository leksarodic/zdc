#include "panels.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "audio_player.h"

#include "theme.h"

#define C_CARD    theme_color(0x1E2128)
#define C_INNER   theme_color(0x262A33)
#define C_BTN     theme_color(0x2A2E37)
#define C_TEXT    theme_color(0xF2F3F5)
#define C_MUTED   theme_color(0x8B9099)
#define C_SOFT    theme_color(0xC5C8CE)
#define C_GREEN   0x2DBE6C

static lv_obj_t *radio_panel;
static lv_obj_t *files_panel;
static lv_obj_t *settings_panel;

static void (*cb_home)(void);
static void (*cb_accent)(uint32_t);
static void (*cb_clock)(bool);
static void (*cb_set_time)(long);
static void (*cb_volume)(int);
static void (*cb_theme)(bool);
static void (*cb_night)(bool);
static void (*cb_saver)(bool);
static void (*cb_radio_power)(bool);
static void (*cb_volume_mute)(bool);
static void (*cb_saver_timeout)(int);

static bool night_on = false;
static bool saver_on = true;
static bool light_on = false;
static int saver_timeout = 300;

static lv_color_t col(uint32_t hex) { return lv_color_hex(hex); }

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

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                        uint32_t bg, const lv_font_t *font, lv_event_cb_t cb, lv_obj_t **out_lbl) {
    lv_obj_t *b = lv_button_create(parent);
    strip(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, col(bg), 0);
    lv_obj_set_style_radius(b, 9, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(C_TEXT), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

static void close_cb(lv_event_t *e) {
    (void)e;
    if (cb_home) cb_home();
}

static lv_obj_t *panel_header(lv_obj_t *panel, const char *title, const char *right, bool show_close) {
    label(panel, title, 18, 12, 260, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *r = NULL;
    if (right && right[0]) {
        r = label(panel, right, 500, 12, 200, 18, &lv_font_montserrat_12, 0xFE3211, LV_TEXT_ALIGN_RIGHT);
    }
    if (show_close) button(panel, LV_SYMBOL_CLOSE, 726, 8, 32, 32, C_INNER, &lv_font_montserrat_12, close_cb, NULL);
    return r;
}

static lv_obj_t *make_panel(lv_obj_t *parent) {
    lv_obj_t *p = box(parent, 12, 50, 776, 318, C_CARD, 16);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
}

/* ── Radio ───────────────────────────────────────────────────── */

static lv_obj_t *radio_freq_lbl;
static lv_obj_t *radio_scale;
static bool radio_scale_sync = false;
static lv_obj_t *radio_hint_lbl;
static lv_obj_t *radio_rds_lbl;
static lv_obj_t *radio_mute_lbl;
static lv_obj_t *radio_power_lbl;
static lv_obj_t *radio_save_lbl;
static lv_obj_t *radio_fm_lbl;
static lv_obj_t *radio_am_lbl;
static lv_obj_t *radio_preset_btns[6];

static bool radio_on = true;
static bool radio_mute = false;
static char radio_summary[96] = "RADIO  off";
static bool radio_am = false;
static bool radio_save_mode = false;

static double radio_fm_freq = 98.1;
static double radio_am_freq = 810.0;

static double presets_fm[6] = {87.5, 90.1, 94.3, 98.1, 101.7, 104.9};
static double presets_am[6] = {540, 630, 720, 810, 900, 990};

static const char *RDS_NAMES[6] = {
    "CAR_482 FM", "Radio One", "Top 40", "Jazz FM", "Rock 101", "News 24"
};

static double radio_freq(void) {
    return radio_am ? radio_am_freq : radio_fm_freq;
}

static double *band_presets(void) {
    return radio_am ? presets_am : presets_fm;
}

static void presets_load(void) {
    FILE *f = fopen("strada.conf", "r");
    if (!f) return;
    double v[12];
    int n = 0;
    while (n < 12 && fscanf(f, "%lf", &v[n]) == 1) n++;
    fclose(f);
    for (int i = 0; i < 6 && i < n; i++) {
        if (v[i] >= 87.5 && v[i] <= 108.0) presets_fm[i] = v[i];
    }
    for (int i = 0; i < 6; i++) {
        int src = 6 + i;
        if (src < n && v[src] >= 520.0 && v[src] <= 1710.0) presets_am[i] = v[src];
    }
}

static void presets_save(void) {
    FILE *f = fopen("strada.conf", "w");
    if (!f) return;
    for (int i = 0; i < 6; i++) fprintf(f, "%.1f\n", presets_fm[i]);
    for (int i = 0; i < 6; i++) fprintf(f, "%.1f\n", presets_am[i]);
    fclose(f);
}

static int rds_slot(void) {
    int best = 0;
    double best_d = 9999.0;
    for (int i = 0; i < 6; i++) {
        double d = fabs(radio_fm_freq - presets_fm[i]);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

static void radio_scale_update(void) {
    if (!radio_scale) return;
    radio_scale_sync = true;
    if (radio_am) {
        lv_slider_set_range(radio_scale, 530, 1700);
        lv_slider_set_value(radio_scale, (int)(radio_am_freq + 0.5), LV_ANIM_OFF);
    } else {
        lv_slider_set_range(radio_scale, 875, 1080);
        lv_slider_set_value(radio_scale, (int)(radio_fm_freq * 10.0 + 0.5), LV_ANIM_OFF);
    }
    radio_scale_sync = false;
}

static void radio_update(void) {
    char buf[48];
    if (!radio_on) snprintf(buf, sizeof(buf), "OFF");
    else if (radio_am) snprintf(buf, sizeof(buf), "%.0f kHz", radio_am_freq);
    else snprintf(buf, sizeof(buf), "%.1f MHz", radio_fm_freq);
    lv_label_set_text(radio_freq_lbl, buf);
    lv_obj_set_style_text_color(radio_freq_lbl, col(radio_on ? C_TEXT : C_MUTED), 0);

    if (radio_save_mode) lv_label_set_text(radio_hint_lbl, "Tap 1-6 to save this station");
    else if (!radio_on) lv_label_set_text(radio_hint_lbl, "Radio off");
    else if (radio_mute) lv_label_set_text(radio_hint_lbl, "Muted");
    else lv_label_set_text(radio_hint_lbl, "");
    lv_obj_set_style_text_color(radio_hint_lbl,
                                col(radio_save_mode ? 0xFE3211 : (radio_on ? C_MUTED : C_MUTED)), 0);

    if (!radio_on) {
        lv_label_set_text(radio_rds_lbl, "RDS  -  off");
        snprintf(radio_summary, sizeof(radio_summary), "RADIO  off");
    } else if (radio_am) {
        lv_label_set_text(radio_rds_lbl, "AM band  -  RDS not available");
        snprintf(radio_summary, sizeof(radio_summary), "RADIO  %.0f kHz", radio_am_freq);
    } else {
        int slot = rds_slot();
        lv_label_set_text_fmt(radio_rds_lbl, "RDS  -  %s", RDS_NAMES[slot]);
        snprintf(radio_summary, sizeof(radio_summary), "RADIO  %.1f MHz  |  %s",
                 radio_fm_freq, RDS_NAMES[slot]);
    }

    lv_label_set_text(radio_mute_lbl, radio_mute ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    if (radio_power_lbl) {
        lv_obj_set_style_bg_color(lv_obj_get_parent(radio_power_lbl),
                                  col(radio_on ? 0xFE3211 : C_INNER), 0);
    }

    lv_obj_set_style_bg_color(lv_obj_get_parent(radio_save_lbl),
                              col(radio_save_mode ? 0xFE3211 : C_INNER), 0);
    lv_obj_set_style_bg_color(lv_obj_get_parent(radio_fm_lbl), col(radio_am ? C_INNER : 0xFE3211), 0);
    lv_obj_set_style_bg_color(lv_obj_get_parent(radio_am_lbl), col(radio_am ? 0xFE3211 : C_INNER), 0);
    lv_obj_set_style_text_color(radio_fm_lbl, col(radio_am ? C_MUTED : 0xFFFFFF), 0);
    lv_obj_set_style_text_color(radio_am_lbl, col(radio_am ? 0xFFFFFF : C_MUTED), 0);

    for (int i = 0; i < 6; i++) {
        double *p = band_presets();
        bool selected = radio_on && fabs(radio_freq() - p[i]) < (radio_am ? 1.0 : 0.05);
        lv_obj_set_style_border_width(radio_preset_btns[i], selected ? 2 : 0, 0);
        lv_obj_set_style_border_color(radio_preset_btns[i], col(0xFE3211), 0);
    }
    radio_scale_update();
}

static void radio_step(double fm_delta, double am_delta) {
    if (!radio_on) return;
    if (radio_am) {
        radio_am_freq += am_delta;
        if (radio_am_freq > 1700.0) radio_am_freq = 530.0;
        if (radio_am_freq < 530.0) radio_am_freq = 1700.0;
    } else {
        radio_fm_freq += fm_delta;
        if (radio_fm_freq > 108.0) radio_fm_freq = 87.5;
        if (radio_fm_freq < 87.5) radio_fm_freq = 108.0;
    }
    radio_update();
}

static void radio_tune_down_cb(lv_event_t *e) { (void)e; radio_step(-0.1, -10.0); }
static void radio_tune_up_cb(lv_event_t *e) { (void)e; radio_step(+0.1, +10.0); }
static void radio_seek_down_cb(lv_event_t *e) { (void)e; radio_step(-1.0, -100.0); }
static void radio_seek_up_cb(lv_event_t *e) { (void)e; radio_step(+1.0, +100.0); }

static void radio_band_cb(lv_event_t *e) {
    bool am = (bool)(intptr_t)lv_event_get_user_data(e);
    if (radio_am == am) return;
    radio_am = am;
    radio_save_mode = false;
    radio_update();
}

static void radio_power_cb(lv_event_t *e) {
    (void)e;
    radio_on = !radio_on;
    if (!radio_on) radio_save_mode = false;
    radio_update();
    if (cb_radio_power) cb_radio_power(radio_on);
}

static void radio_scale_cb(lv_event_t *e) {
    (void)e;
    if (radio_scale_sync || !radio_on) return;
    int v = (int)lv_slider_get_value(radio_scale);
    if (radio_am) radio_am_freq = (double)v;
    else radio_fm_freq = (double)v / 10.0;
    radio_update();
}

static void radio_mute_cb(lv_event_t *e) {
    (void)e;
    radio_mute = !radio_mute;
    radio_update();
    if (cb_volume_mute) cb_volume_mute(radio_mute);
}

static void radio_preset_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 6) return;
    double *p = band_presets();
    if (radio_save_mode) {
        p[idx] = radio_freq();
        radio_save_mode = false;
        presets_save();
    } else {
        radio_on = true;
        if (radio_am) radio_am_freq = p[idx];
        else radio_fm_freq = p[idx];
    }
    radio_update();
}

static void radio_save_cb(lv_event_t *e) {
    (void)e;
    radio_save_mode = !radio_save_mode;
    radio_update();
}

static void build_radio(lv_obj_t *parent) {
    radio_panel = make_panel(parent);
    panel_header(radio_panel, "RADIO", "", false);

    radio_freq_lbl = label(radio_panel, "98.1 MHz", 0, 30, 776, 64,
                           &lv_font_montserrat_40, C_TEXT, LV_TEXT_ALIGN_CENTER);
    radio_hint_lbl = label(radio_panel, "", 0, 94, 776, 14,
                           &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_CENTER);
    radio_rds_lbl = label(radio_panel, "RDS  -  off", 0, 110, 776, 14,
                          &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_CENTER);

    radio_scale = lv_slider_create(radio_panel);
    lv_obj_set_pos(radio_scale, 90, 128);
    lv_obj_set_size(radio_scale, 596, 18);
    lv_slider_set_range(radio_scale, 875, 1080);
    lv_slider_set_value(radio_scale, 981, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(radio_scale, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(radio_scale, col(0xFE3211), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(radio_scale, col(0xFE3211), LV_PART_KNOB);
    lv_obj_add_event_cb(radio_scale, radio_scale_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *fm = button(radio_panel, "FM", 650, 34, 54, 28, C_BTN,
                          &lv_font_montserrat_12, NULL, &radio_fm_lbl);
    lv_obj_add_event_cb(fm, radio_band_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_t *am = button(radio_panel, "AM", 710, 34, 54, 28, C_INNER,
                          &lv_font_montserrat_12, NULL, &radio_am_lbl);
    lv_obj_add_event_cb(am, radio_band_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    (void)fm; (void)am;

    button(radio_panel, LV_SYMBOL_LEFT, 96, 158, 84, 38, C_INNER,
           &lv_font_montserrat_18, radio_tune_down_cb, NULL);
    button(radio_panel, LV_SYMBOL_PREV, 204, 158, 84, 38, C_BTN,
           &lv_font_montserrat_16, radio_seek_down_cb, NULL);
    button(radio_panel, LV_SYMBOL_NEXT, 488, 158, 84, 38, C_BTN,
           &lv_font_montserrat_16, radio_seek_up_cb, NULL);
    button(radio_panel, LV_SYMBOL_RIGHT, 596, 158, 84, 38, C_INNER,
           &lv_font_montserrat_18, radio_tune_up_cb, NULL);

    label(radio_panel, "PRESETS", 40, 202, 300, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 6; i++) {
        char b[8];
        snprintf(b, sizeof(b), "%d", i + 1);
        radio_preset_btns[i] = button(radio_panel, b, 40 + i * 120, 220, 104, 32, C_INNER,
                                      &lv_font_montserrat_12, NULL, NULL);
        lv_obj_add_event_cb(radio_preset_btns[i], radio_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    button(radio_panel, LV_SYMBOL_POWER, 80, 266, 90, 34, 0xFE3211,
           &lv_font_montserrat_18, radio_power_cb, &radio_power_lbl);
    button(radio_panel, LV_SYMBOL_SAVE, 238, 266, 160, 34, C_INNER,
           &lv_font_montserrat_16, radio_save_cb, &radio_save_lbl);
    button(radio_panel, LV_SYMBOL_MUTE, 508, 266, 90, 34, C_INNER,
           &lv_font_montserrat_16, radio_mute_cb, &radio_mute_lbl);

    presets_load();
    radio_update();
}

/* ── Files ───────────────────────────────────────────────────── */

#define FILE_MAX 64
#define STRADA_PATH_MAX 512

typedef struct {
    char name[256];
    char full[STRADA_PATH_MAX];
    bool is_dir;
    long size;
} file_entry_t;

typedef enum { FILTER_ALL = 0, FILTER_MUSIC = 1, FILTER_PHOTOS = 2 } file_filter_t;

#define FILE_ROOTS_MAX 4
static char file_root[STRADA_PATH_MAX] = "media";
static char file_roots[FILE_ROOTS_MAX][STRADA_PATH_MAX];
static char file_root_labels[FILE_ROOTS_MAX][16];
static int file_root_count = 1;
static int file_root_index = 0;
static char file_cwd[STRADA_PATH_MAX] = "";
static file_entry_t file_entries[FILE_MAX];
static int file_entry_count = 0;
static file_filter_t file_filter = FILTER_ALL;

static lv_obj_t *files_path_lbl;
static lv_obj_t *files_status_lbl;
static lv_obj_t *files_info_lbl;
static lv_obj_t *files_list;
static lv_obj_t *file_filter_btns[3];
static lv_obj_t *files_source_btns[FILE_ROOTS_MAX];
static lv_obj_t *file_preview;
static lv_obj_t *file_preview_title;
static lv_obj_t *file_preview_text;
static lv_obj_t *file_preview_img;
static char preview_src[STRADA_PATH_MAX + 4];

static lv_obj_t *music_box;
static lv_obj_t *music_title;
static lv_obj_t *music_bar;
static lv_obj_t *music_play_lbl;
static lv_timer_t *music_timer;
static int music_duration = 0;
static int music_elapsed = 0;
static bool music_playing = false;
static bool music_real = false;

static bool ext_in(const char *name, const char *const *exts) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    dot++;
    for (int i = 0; exts[i]; i++) {
        if (!strcasecmp(dot, exts[i])) return true;
    }
    return false;
}

static bool is_music(const char *name) {
    static const char *const exts[] = {"mp3", "wav", "ogg", "flac", "m4a", "aac", "wma", NULL};
    return ext_in(name, exts);
}

static bool is_photo(const char *name) {
    static const char *const exts[] = {"png", "jpg", "jpeg", "bmp", "gif", NULL};
    return ext_in(name, exts);
}

static int file_cmp(const void *a, const void *b) {
    const file_entry_t *x = a, *y = b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcasecmp(x->name, y->name);
}

static void file_preview_close_cb(lv_event_t *e) {
    (void)e;
    lv_obj_add_flag(file_preview, LV_OBJ_FLAG_HIDDEN);
}

static void show_preview(const file_entry_t *ent) {
    if (!file_preview) {
        file_preview = box(files_panel, 30, 40, 716, 220, C_INNER, 12);
        file_preview_title = label(file_preview, "", 16, 12, 620, 18,
                                   &lv_font_montserrat_14, C_TEXT, LV_TEXT_ALIGN_LEFT);
        button(file_preview, LV_SYMBOL_CLOSE, 668, 8, 36, 32, C_BTN, &lv_font_montserrat_12,
               file_preview_close_cb, NULL);
        file_preview_text = lv_label_create(file_preview);
        lv_label_set_long_mode(file_preview_text, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_pos(file_preview_text, 16, 44);
        lv_obj_set_size(file_preview_text, 680, 160);
        lv_obj_set_style_text_font(file_preview_text, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(file_preview_text, col(C_SOFT), 0);

        file_preview_img = lv_image_create(file_preview);
        lv_obj_set_pos(file_preview_img, 18, 44);
        lv_obj_set_size(file_preview_img, 680, 158);
        lv_image_set_inner_align(file_preview_img, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_add_flag(file_preview_img, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(file_preview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(file_preview);
    lv_label_set_text(file_preview_title, ent->name);
    lv_obj_set_style_text_color(file_preview_title, col(C_TEXT), 0);

    if (is_photo(ent->name)) {
        lv_image_cache_drop(preview_src);
        snprintf(preview_src, sizeof(preview_src), "A:%s", ent->full);
        lv_image_header_t hdr;
        if (lv_image_decoder_get_info(preview_src, &hdr) == LV_RESULT_OK) {
            lv_obj_add_flag(file_preview_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(file_preview_img, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(file_preview_img, preview_src);
            return;
        }
        lv_obj_set_style_text_color(file_preview_title, col(0xFE3211), 0);
        lv_label_set_text(file_preview_text, "Cannot decode this image.\n\nSupported by the emulator: BMP, PNG, JPG.");
        lv_obj_add_flag(file_preview_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(file_preview_text, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_image_cache_drop(preview_src);
    lv_obj_add_flag(file_preview_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(file_preview_text, LV_OBJ_FLAG_HIDDEN);

    char text[2200];
    text[0] = '\0';
    FILE *f = fopen(ent->full, "rb");
    if (!f) {
        snprintf(text, sizeof(text), "Cannot open file.\n%s", ent->full);
    } else {
        size_t n = fread(text, 1, sizeof(text) - 1, f);
        text[n] = '\0';
        fclose(f);
        bool binary = false;
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)text[i];
            if (c == 0 || (c < 9) || (c > 13 && c < 32)) { binary = true; break; }
        }
        if (binary) {
            snprintf(text, sizeof(text), "Binary file\n\nName: %s\nSize: %ld bytes", ent->name, ent->size);
        } else if (n == 0) {
            snprintf(text, sizeof(text), "(empty file)");
        }
    }
    lv_label_set_text(file_preview_text, text);
}

static void music_update(void) {
    if (!music_box) return;
    lv_bar_set_range(music_bar, 0, music_duration > 0 ? music_duration : 1);
    lv_bar_set_value(music_bar, music_elapsed, LV_ANIM_OFF);
    lv_label_set_text(music_play_lbl, music_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void music_tick_cb(lv_timer_t *t) {
    (void)t;
    if (!music_playing || !music_box) return;
    if (music_elapsed < music_duration) music_elapsed++;
    if (music_elapsed >= music_duration) {
        music_playing = false;
        audio_player_pause(true);
    }
    lv_bar_set_value(music_bar, music_elapsed, LV_ANIM_OFF);
    lv_label_set_text(music_play_lbl, music_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void music_toggle_cb(lv_event_t *e) {
    (void)e;
    music_playing = !music_playing;
    audio_player_pause(!music_playing);
    lv_label_set_text(music_play_lbl, music_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void music_stop_cb(lv_event_t *e) {
    (void)e;
    music_playing = false;
    audio_player_stop();
    if (music_box) lv_obj_add_flag(music_box, LV_OBJ_FLAG_HIDDEN);
}

static void music_start(const file_entry_t *ent) {
    audio_player_stop();
    int duration = 0;
    music_real = audio_player_play(ent->full, &duration);
    music_duration = duration > 0 ? duration : 180;
    music_elapsed = 0;
    music_playing = true;
    if (!music_box) return;
    lv_label_set_text_fmt(music_title, "%s%s", ent->name, music_real ? "" : "  (simulated)");
    lv_obj_remove_flag(music_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(music_box);
    music_update();
}

static void load_dir(const char *path);

static void file_row_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= file_entry_count) return;
    file_entry_t *ent = &file_entries[idx];
    if (ent->is_dir) {
        load_dir(ent->full);
    } else if (is_music(ent->name)) {
        music_start(ent);
    } else {
        show_preview(ent);
    }
}

static void files_filter_refresh(void) {
    for (int i = 0; i < 3; i++) {
        bool selected = (int)file_filter == i;
        lv_obj_set_style_bg_color(file_filter_btns[i], col(selected ? 0xFE3211 : C_INNER), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(file_filter_btns[i], 0),
                                    col(selected ? 0xFFFFFF : C_SOFT), 0);
    }
}

static void files_filter_cb(lv_event_t *e) {
    int f = (int)(intptr_t)lv_event_get_user_data(e);
    file_filter = (file_filter_t)f;
    files_filter_refresh();
    if (file_filter == FILTER_MUSIC || file_filter == FILTER_PHOTOS) {
        char target[STRADA_PATH_MAX];
        snprintf(target, sizeof(target), "%s/%s", file_root,
                 file_filter == FILTER_MUSIC ? "Music" : "Photos");
        DIR *d = opendir(target);
        if (d) {
            closedir(d);
            load_dir(target);
            return;
        }
    }
    load_dir(file_root);
}

static void load_dir(const char *path) {
    file_entry_count = 0;
    DIR *d = opendir(path);
    if (!d) {
        bool at_root = strcmp(path, file_root) == 0;
        lv_label_set_text(files_status_lbl, at_root ? "NO STORAGE" : "NOT AVAILABLE");
        lv_label_set_text(files_path_lbl, at_root ? "No USB or SD card connected" : path);
        lv_obj_clean(files_list);
        label(files_list, at_root ? "No USB or SD card connected" : "Folder not available",
              20, 20, 700, 24, &lv_font_montserrat_18, C_TEXT, LV_TEXT_ALIGN_LEFT);
        label(files_list,
              at_root ? "Insert a USB drive or SD card, then press Refresh."
                      : "Go back and try another folder.",
              20, 54, 700, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
        lv_label_set_text(files_info_lbl, "0 items");
        if (at_root) file_cwd[0] = '\0';
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && file_entry_count < FILE_MAX) {
        if (de->d_name[0] == '.') continue;
        struct stat st;
        char full[STRADA_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
        if (stat(full, &st) != 0) continue;
        bool dir = S_ISDIR(st.st_mode);
        if (file_filter == FILTER_MUSIC && (dir || !is_music(de->d_name))) continue;
        if (file_filter == FILTER_PHOTOS && (dir || !is_photo(de->d_name))) continue;
        file_entry_t *e = &file_entries[file_entry_count];
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        snprintf(e->full, sizeof(e->full), "%s", full);
        e->is_dir = dir;
        e->size = (long)st.st_size;
        file_entry_count++;
    }
    closedir(d);

    qsort(file_entries, (size_t)file_entry_count, sizeof(file_entry_t), file_cmp);
    snprintf(file_cwd, sizeof(file_cwd), "%s", path);
    lv_label_set_text(files_path_lbl, file_cwd);
    lv_label_set_text(files_status_lbl,
                      file_root_count > 1 ? "USB + SD" : file_root_labels[0]);

    lv_obj_clean(files_list);
    if (file_entry_count == 0) {
        if (file_filter == FILTER_MUSIC) {
            label(files_list, "No music files", 20, 20, 700, 24,
                  &lv_font_montserrat_18, C_TEXT, LV_TEXT_ALIGN_LEFT);
            label(files_list, "Copy MP3, WAV, FLAC or OGG files to the Music folder.", 20, 54, 700, 18,
                  &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
        } else if (file_filter == FILTER_PHOTOS) {
            label(files_list, "No photos", 20, 20, 700, 24,
                  &lv_font_montserrat_18, C_TEXT, LV_TEXT_ALIGN_LEFT);
            label(files_list, "Copy BMP, PNG or JPG files to the Photos folder.", 20, 54, 700, 18,
                  &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
        } else {
            label(files_list, "USB/SD is empty", 20, 20, 700, 24,
                  &lv_font_montserrat_18, C_TEXT, LV_TEXT_ALIGN_LEFT);
            label(files_list, "Copy files to the drive and press Refresh.", 20, 54, 700, 18,
                  &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
        }
        lv_label_set_text(files_info_lbl, "0 items");
        return;
    }
    for (int i = 0; i < file_entry_count; i++) {
        file_entry_t *e = &file_entries[i];
        lv_obj_t *row = lv_button_create(files_list);
        strip(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 27);
        lv_obj_set_style_bg_color(row, col(C_INNER), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_left(row, 8, 0);

        const char *icon = e->is_dir ? LV_SYMBOL_DIRECTORY
                          : is_music(e->name) ? LV_SYMBOL_AUDIO
                          : is_photo(e->name) ? LV_SYMBOL_IMAGE
                          : LV_SYMBOL_FILE;
        lv_obj_t *l = lv_label_create(row);
        if (e->is_dir) {
            lv_label_set_text_fmt(l, "%s  %s", icon, e->name);
        } else {
            lv_label_set_text_fmt(l, "%s  %s   %ld B", icon, e->name, e->size);
        }
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(l, col(e->is_dir ? C_TEXT : C_SOFT), 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_add_event_cb(row, file_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_label_set_text_fmt(files_info_lbl, "%d items  /  %s", file_entry_count, file_cwd);
}

static void files_up_cb(lv_event_t *e) {
    (void)e;
    if (!file_cwd[0] || strcmp(file_cwd, file_root) == 0) return;
    char parent[STRADA_PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", file_cwd);
    char *slash = strrchr(parent, '/');
    if (!slash) return;
    if (slash == parent) parent[1] = '\0';
    else *slash = '\0';
    load_dir(parent);
}

static void files_refresh_cb(lv_event_t *e) {
    (void)e;
    load_dir(file_cwd[0] ? file_cwd : file_root);
}

static void files_source_refresh(void) {
    for (int i = 0; i < file_root_count && i < FILE_ROOTS_MAX; i++) {
        if (!files_source_btns[i]) continue;
        bool sel = i == file_root_index;
        lv_obj_set_style_bg_color(files_source_btns[i], col(sel ? 0xFE3211 : C_INNER), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(files_source_btns[i], 0),
                                    col(sel ? 0xFFFFFF : C_SOFT), 0);
        char txt[32];
        if (sel) snprintf(txt, sizeof(txt), "%s %s", LV_SYMBOL_OK, file_root_labels[i]);
        else snprintf(txt, sizeof(txt), "%s", file_root_labels[i]);
        lv_label_set_text(lv_obj_get_child(files_source_btns[i], 0), txt);
    }
}

static void files_select_root(int idx) {
    if (idx < 0 || idx >= file_root_count || idx == file_root_index) return;
    file_root_index = idx;
    snprintf(file_root, sizeof(file_root), "%s", file_roots[idx]);
    file_cwd[0] = '\0';
    file_filter = FILTER_ALL;
    files_filter_refresh();
    files_source_refresh();
    load_dir(file_root);
}

static void files_source_cb(lv_event_t *e) {
    files_select_root((int)(intptr_t)lv_event_get_user_data(e));
}

static void build_files(lv_obj_t *parent) {
    files_panel = make_panel(parent);
    files_status_lbl = panel_header(files_panel, "FILES",
                                    file_root_count > 1 ? "USB + SD" : "USB / SD", true);

    label(files_panel, "SOURCE", 18, 40, 52, 12, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < file_root_count && i < FILE_ROOTS_MAX; i++) {
        files_source_btns[i] = button(files_panel, file_root_labels[i], 76 + i * 82, 34, 74, 24,
                                      C_INNER, &lv_font_montserrat_10, NULL, NULL);
        lv_obj_add_event_cb(files_source_btns[i], files_source_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    files_path_lbl = label(files_panel, file_root, 18, 62, 700, 16,
                           &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(files_path_lbl, LV_LABEL_LONG_MODE_CLIP);

    static const char *FILTERS[] = {"All", "Music", "Photos"};
    static const int FILTER_X[] = {18, 86, 166};
    static const int FILTER_W[] = {62, 74, 80};
    for (int i = 0; i < 3; i++) {
        file_filter_btns[i] = button(files_panel, FILTERS[i], FILTER_X[i], 82, FILTER_W[i], 30,
                                     C_INNER, &lv_font_montserrat_10, NULL, NULL);
        lv_obj_add_event_cb(file_filter_btns[i], files_filter_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    button(files_panel, LV_SYMBOL_UP, 610, 82, 54, 30, C_INNER, &lv_font_montserrat_12, files_up_cb, NULL);
    button(files_panel, LV_SYMBOL_REFRESH, 672, 82, 54, 30, C_INNER, &lv_font_montserrat_12, files_refresh_cb, NULL);

    files_list = box(files_panel, 18, 116, 740, 128, C_CARD, 8);
    lv_obj_set_style_bg_opa(files_list, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(files_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(files_list, 4, 0);
    lv_obj_set_style_pad_all(files_list, 2, 0);
    lv_obj_set_scroll_dir(files_list, LV_DIR_VER);

    files_info_lbl = label(files_panel, "", 18, 246, 740, 18,
                           &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(files_info_lbl, LV_LABEL_LONG_MODE_CLIP);

    music_box = box(files_panel, 18, 266, 740, 38, C_INNER, 8);
    lv_obj_add_flag(music_box, LV_OBJ_FLAG_HIDDEN);
    music_title = label(music_box, "", 10, 3, 520, 14, &lv_font_montserrat_10, C_TEXT,
                        LV_TEXT_ALIGN_LEFT);
    music_bar = lv_bar_create(music_box);
    lv_obj_set_pos(music_bar, 10, 23);
    lv_obj_set_size(music_bar, 520, 6);
    lv_bar_set_range(music_bar, 0, 180);
    lv_bar_set_value(music_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(music_bar, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_bar, col(0xFE3211), LV_PART_INDICATOR);
    lv_obj_set_style_radius(music_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(music_bar, 3, LV_PART_INDICATOR);
    button(music_box, LV_SYMBOL_PAUSE, 548, 4, 34, 30, C_BTN, &lv_font_montserrat_12,
           music_toggle_cb, &music_play_lbl);
    button(music_box, LV_SYMBOL_CLOSE, 592, 4, 34, 30, C_BTN, &lv_font_montserrat_12,
           music_stop_cb, NULL);

    music_timer = lv_timer_create(music_tick_cb, 1000, NULL);
    (void)music_timer;

    files_filter_refresh();
    files_source_refresh();
    load_dir(file_root);
}

/* ── Settings ────────────────────────────────────────────────── */

static const uint32_t ACCENTS[5] = {0xFE3211, 0xFF7A1A, 0x2DBE6C, 0x2F7CF6, 0x8E5CF7};
static int accent_idx = 0;
static lv_obj_t *accent_btns[5];
static lv_obj_t *clock24_lbl;
static lv_obj_t *clock12_lbl;
static lv_obj_t *volume_lbl;
static lv_obj_t *volume_slider;
static bool clock_is_24 = true;
static lv_obj_t *theme_dark_lbl;
static lv_obj_t *theme_light_lbl;
static lv_obj_t *night_lbl;
static lv_obj_t *saver_lbl;
static lv_obj_t *timeout_btns[3];

static struct tm cal;
static bool cal_ready = false;
static lv_obj_t *date_lbl;
static lv_obj_t *time_lbl;

static void cal_load_now(void) {
    time_t now = time(NULL);
    localtime_r(&now, &cal);
    cal.tm_isdst = -1;
    cal_ready = true;
}

static void cal_show(void) {
    if (!cal_ready) cal_load_now();
    char d[32], t[16];
    strftime(d, sizeof(d), "%Y-%m-%d", &cal);
    strftime(t, sizeof(t), "%H:%M", &cal);
    if (date_lbl) lv_label_set_text(date_lbl, d);
    if (time_lbl) lv_label_set_text(time_lbl, t);
}

static void cal_commit(void) {
    cal.tm_isdst = -1;
    time_t epoch = mktime(&cal);
    if (epoch == (time_t)-1) return;
    struct tm norm;
    localtime_r(&epoch, &norm);
    cal = norm;
    cal.tm_isdst = -1;
    if (cb_set_time) cb_set_time((long)epoch);
    cal_show();
}

static void cal_adjust(int code) {
    bool plus = (code & 1) != 0;
    int field = code >> 1;
    int delta = plus ? 1 : -1;
    if (!cal_ready) cal_load_now();
    switch (field) {
    case 0: cal.tm_year += delta; break;
    case 1: cal.tm_mon += delta; break;
    case 2: cal.tm_mday += delta; break;
    case 3: cal.tm_hour += delta; break;
    case 4: cal.tm_min += delta; break;
    default: return;
    }
    cal_commit();
}

static void cal_btn_cb(lv_event_t *e) {
    cal_adjust((int)(intptr_t)lv_event_get_user_data(e));
}

static void settings_refresh(void) {
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_border_width(accent_btns[i], i == accent_idx ? 3 : 0, 0);
        lv_obj_set_style_border_color(accent_btns[i], col(C_TEXT), 0);
    }
    if (clock24_lbl) {
        lv_obj_set_style_bg_color(lv_obj_get_parent(clock24_lbl),
                                  col(clock_is_24 ? C_BTN : C_INNER), 0);
        lv_obj_set_style_bg_color(lv_obj_get_parent(clock12_lbl),
                                  col(clock_is_24 ? C_INNER : C_BTN), 0);
    }
    if (volume_slider) {
        int v = lv_slider_get_value(volume_slider);
        lv_label_set_text_fmt(volume_lbl, "%d%%", v);
    }
    if (theme_dark_lbl) {
        lv_obj_set_style_bg_color(lv_obj_get_parent(theme_dark_lbl),
                                  col(light_on ? C_INNER : C_BTN), 0);
        lv_obj_set_style_bg_color(lv_obj_get_parent(theme_light_lbl),
                                  col(light_on ? C_BTN : C_INNER), 0);
        lv_obj_set_style_border_width(lv_obj_get_parent(theme_dark_lbl), light_on ? 0 : 2, 0);
        lv_obj_set_style_border_width(lv_obj_get_parent(theme_light_lbl), light_on ? 2 : 0, 0);
        lv_obj_set_style_border_color(lv_obj_get_parent(theme_dark_lbl), col(ACCENTS[accent_idx]), 0);
        lv_obj_set_style_border_color(lv_obj_get_parent(theme_light_lbl), col(ACCENTS[accent_idx]), 0);
        lv_label_set_text(night_lbl, night_on ? "Dim on" : "Dim off");
        lv_label_set_text(saver_lbl, saver_on ? "Saver on" : "Saver off");
        if (timeout_btns[0]) {
            static const int STEPS[] = {60, 300, 1800};
            for (int i = 0; i < 3; i++) {
                bool sel = saver_timeout == STEPS[i];
                lv_obj_set_style_bg_color(timeout_btns[i], col(sel ? 0xFE3211 : C_INNER), 0);
                lv_obj_set_style_text_color(lv_obj_get_child(timeout_btns[i], 0),
                                            col(sel ? 0xFFFFFF : C_SOFT), 0);
            }
        }
    }
    cal_show();
}

static void accent_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 5) return;
    accent_idx = idx;
    if (volume_slider) {
        lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[idx]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[idx]), LV_PART_KNOB);
    }
    if (cb_accent) cb_accent(ACCENTS[idx]);
    settings_refresh();
}

static void clock24_cb(lv_event_t *e) {
    (void)e;
    clock_is_24 = true;
    if (cb_clock) cb_clock(true);
    settings_refresh();
}

static void clock12_cb(lv_event_t *e) {
    (void)e;
    clock_is_24 = false;
    if (cb_clock) cb_clock(false);
    settings_refresh();
}

static void volume_cb(lv_event_t *e) {
    (void)e;
    settings_refresh();
    if (cb_volume) cb_volume(lv_slider_get_value(volume_slider));
}

static void theme_dark_cb(lv_event_t *e) {
    (void)e;
    light_on = false;
    if (cb_theme) cb_theme(false);
    settings_refresh();
}

static void theme_light_cb(lv_event_t *e) {
    (void)e;
    light_on = true;
    if (cb_theme) cb_theme(true);
    settings_refresh();
}

static void night_cb(lv_event_t *e) {
    (void)e;
    night_on = !night_on;
    if (cb_night) cb_night(night_on);
    settings_refresh();
}

static void saver_cb(lv_event_t *e) {
    (void)e;
    saver_on = !saver_on;
    if (cb_saver) cb_saver(saver_on);
    settings_refresh();
}

static void timeout_cb(lv_event_t *e) {
    int seconds = (int)(intptr_t)lv_event_get_user_data(e);
    if (seconds != 60 && seconds != 300 && seconds != 1800) return;
    saver_timeout = seconds;
    if (cb_saver_timeout) cb_saver_timeout(saver_timeout);
    settings_refresh();
}

static lv_obj_t *reset_confirm;
static lv_obj_t *disclaimer_box;

static void reset_cancel_cb(lv_event_t *e) {
    (void)e;
    if (reset_confirm) lv_obj_add_flag(reset_confirm, LV_OBJ_FLAG_HIDDEN);
}

static void reset_ask_cb(lv_event_t *e) {
    (void)e;
    if (!reset_confirm) return;
    lv_obj_remove_flag(reset_confirm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(reset_confirm);
}

static void disclaimer_close_cb(lv_event_t *e) {
    (void)e;
    if (disclaimer_box) lv_obj_add_flag(disclaimer_box, LV_OBJ_FLAG_HIDDEN);
}

static void disclaimer_open_cb(lv_event_t *e) {
    (void)e;
    if (!disclaimer_box) return;
    lv_obj_remove_flag(disclaimer_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(disclaimer_box);
}

static void reset_apply_cb(lv_event_t *e) {
    (void)e;
    if (reset_confirm) lv_obj_add_flag(reset_confirm, LV_OBJ_FLAG_HIDDEN);
    accent_idx = 0;
    clock_is_24 = true;
    lv_slider_set_value(volume_slider, 15, LV_ANIM_OFF);
    if (cb_volume) cb_volume(15);
    cal_load_now();
    if (cb_set_time) cb_set_time((long)time(NULL));
    if (cb_accent) cb_accent(ACCENTS[0]);
    if (cb_clock) cb_clock(true);
    light_on = false;
    night_on = false;
    saver_on = true;
    saver_timeout = 300;
    if (cb_theme) cb_theme(false);
    if (cb_night) cb_night(false);
    if (cb_saver) cb_saver(true);
    if (cb_saver_timeout) cb_saver_timeout(300);
    settings_refresh();
}

static void build_settings(lv_obj_t *parent) {
    settings_panel = make_panel(parent);
    panel_header(settings_panel, "SETTINGS", "CAR_482", false);

    label(settings_panel, "ACCENT", 24, 44, 90, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = box(settings_panel, 120 + i * 46, 38, 32, 32, ACCENTS[i], LV_RADIUS_CIRCLE);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        accent_btns[i] = b;
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, accent_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    label(settings_panel, "CLOCK", 24, 80, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *b24 = button(settings_panel, "24 h", 120, 74, 70, 28, C_BTN, &lv_font_montserrat_12, clock24_cb, &clock24_lbl);
    lv_obj_t *b12 = button(settings_panel, "12 h", 198, 74, 70, 28, C_INNER, &lv_font_montserrat_12, clock12_cb, &clock12_lbl);
    (void)b24; (void)b12;

    label(settings_panel, "VOLUME ON BOOT", 24, 114, 100, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    volume_slider = lv_slider_create(settings_panel);
    lv_obj_set_pos(volume_slider, 132, 110);
    lv_obj_set_size(volume_slider, 288, 20);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 15, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[0]), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[0]), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider, volume_cb, LV_EVENT_VALUE_CHANGED, NULL);
    volume_lbl = label(settings_panel, "15%", 436, 110, 60, 20,
                       &lv_font_montserrat_12, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "DATE", 24, 148, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    date_lbl = label(settings_panel, "2026-11-26", 120, 148, 100, 20,
                     &lv_font_montserrat_12, C_TEXT, LV_TEXT_ALIGN_LEFT);
    static const char *DBTN[] = {"Y-", "Y+", "M-", "M+", "D-", "D+"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *b = button(settings_panel, DBTN[i], 230 + i * 46, 146, 40, 26, C_INNER,
                             &lv_font_montserrat_10, NULL, NULL);
        lv_obj_add_event_cb(b, cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    label(settings_panel, "TIME", 24, 182, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    time_lbl = label(settings_panel, "15:40", 120, 182, 80, 20,
                     &lv_font_montserrat_12, C_TEXT, LV_TEXT_ALIGN_LEFT);
    static const char *TBTN[] = {"H-", "H+", "m-", "m+"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = button(settings_panel, TBTN[i], 230 + i * 46, 180, 40, 26, C_INNER,
                             &lv_font_montserrat_10, NULL, NULL);
        lv_obj_add_event_cb(b, cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(6 + i));
    }
    label(settings_panel, "THEME", 552, 44, 52, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *td = button(settings_panel, "Dark", 610, 38, 66, 28, C_BTN, &lv_font_montserrat_10,
                          theme_dark_cb, &theme_dark_lbl);
    lv_obj_t *tl = button(settings_panel, "Light", 682, 38, 66, 28, C_INNER, &lv_font_montserrat_10,
                          theme_light_cb, &theme_light_lbl);
    (void)td; (void)tl;

    label(settings_panel, "NIGHT", 552, 80, 52, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(settings_panel, "Dim off", 610, 74, 138, 28, C_INNER, &lv_font_montserrat_10,
           night_cb, &night_lbl);

    label(settings_panel, "IDLE", 552, 114, 52, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(settings_panel, "Saver on", 610, 108, 138, 28, C_INNER, &lv_font_montserrat_10,
           saver_cb, &saver_lbl);

    label(settings_panel, "AFTER", 552, 148, 52, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    static const char *TIMEOUT_TXT[] = {"1m", "5m", "30m"};
    static const int TIMEOUT_SEC[] = {60, 300, 1800};
    for (int i = 0; i < 3; i++) {
        timeout_btns[i] = button(settings_panel, TIMEOUT_TXT[i], 610 + i * 46, 142, 42, 28,
                                 C_INNER, &lv_font_montserrat_10, NULL, NULL);
        lv_obj_add_event_cb(timeout_btns[i], timeout_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)TIMEOUT_SEC[i]);
    }

    label(settings_panel, "ABOUT", 24, 214, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "CAR_482 / v0.9.5 / LVGL", 120, 214, 320, 14,
          &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "BT 55 AA LEN CMD DATA CHK", 120, 230, 320, 14,
          &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);
    button(settings_panel, "Disclaimer", 536, 252, 104, 26, C_INNER, &lv_font_montserrat_10,
           disclaimer_open_cb, NULL);
    button(settings_panel, "Reset UI", 652, 252, 96, 26, C_INNER, &lv_font_montserrat_10,
           reset_ask_cb, NULL);

    reset_confirm = box(settings_panel, 220, 78, 336, 140, C_CARD, 14);
    lv_obj_set_style_border_width(reset_confirm, 1, 0);
    lv_obj_set_style_border_color(reset_confirm, col(C_BTN), 0);
    lv_obj_add_flag(reset_confirm, LV_OBJ_FLAG_HIDDEN);
    label(reset_confirm, "Reset UI?", 20, 16, 296, 24, &lv_font_montserrat_18, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(reset_confirm, "This resets theme, layout, volume and date/time settings. Do you agree?",
          20, 46, 296, 36, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(reset_confirm, "No", 64, 96, 80, 28, C_INNER, &lv_font_montserrat_10, reset_cancel_cb, NULL);
    button(reset_confirm, "Yes, reset", 172, 96, 100, 28, 0xFE3211, &lv_font_montserrat_10, reset_apply_cb, NULL);

    disclaimer_box = box(settings_panel, 40, 40, 696, 238, C_CARD, 14);
    lv_obj_set_style_border_width(disclaimer_box, 1, 0);
    lv_obj_set_style_border_color(disclaimer_box, col(C_BTN), 0);
    lv_obj_add_flag(disclaimer_box, LV_OBJ_FLAG_HIDDEN);
    label(disclaimer_box, "DISCLAIMER", 20, 14, 300, 16, &lv_font_montserrat_12, 0xFE3211, LV_TEXT_ALIGN_LEFT);
    button(disclaimer_box, LV_SYMBOL_CLOSE, 650, 8, 30, 30, C_INNER, &lv_font_montserrat_12,
           disclaimer_close_cb, NULL);
    lv_obj_t *dtxt = lv_label_create(disclaimer_box);
    lv_label_set_text(dtxt,
        "Experimental project. Flashing or modifying firmware can brick the device, erase data "
        "or break radio/Bluetooth. Use it entirely at your own risk. The authors and contributors "
        "are not responsible or liable for any damage, data loss, injury or legal issue. "
        "Do not operate the UI while driving.");
    lv_label_set_long_mode(dtxt, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(dtxt, 20, 42);
    lv_obj_set_size(dtxt, 656, 176);
    lv_obj_set_style_text_font(dtxt, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(dtxt, col(C_SOFT), 0);
    cal_load_now();
    cal_show();
    settings_refresh();
}

/* ── public API ──────────────────────────────────────────────── */

void panels_init(lv_obj_t *parent, void (*on_home)(void), void (*on_accent)(uint32_t), void (*on_clock)(bool), void (*on_set_time)(long), void (*on_volume)(int), void (*on_theme)(bool), void (*on_night)(bool), void (*on_saver)(bool), void (*on_radio_power)(bool), void (*on_volume_mute)(bool), void (*on_saver_timeout)(int)) {
    cb_home = on_home;
    cb_accent = on_accent;
    cb_clock = on_clock;
    cb_set_time = on_set_time;
    cb_volume = on_volume;
    cb_theme = on_theme;
    cb_night = on_night;
    cb_saver = on_saver;
    cb_radio_power = on_radio_power;
    cb_volume_mute = on_volume_mute;
    cb_saver_timeout = on_saver_timeout;

    build_radio(parent);
    build_files(parent);
    build_settings(parent);
}

void panels_set_boot_volume(int value) {
    if (!volume_slider) return;
    lv_slider_set_value(volume_slider, value, LV_ANIM_OFF);
    if (volume_lbl) lv_label_set_text_fmt(volume_lbl, "%d%%", value);
}

void panels_set_saver_timeout(int seconds) {
    saver_timeout = seconds;
    if (settings_panel) settings_refresh();
}

const char *panels_radio_summary(void) {
    return radio_summary;
}

bool panels_radio_on(void) {
    return radio_on;
}

void panels_set_radio_mute(bool muted) {
    radio_mute = muted;
    radio_update();
    if (cb_volume_mute) cb_volume_mute(muted);
}

void panels_open_file(const char *path) {
    if (!path) return;
    char dir[STRADA_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else snprintf(dir, sizeof(dir), "%s", file_root);
    file_filter = FILTER_ALL;
    files_filter_refresh();
    load_dir(dir);
    for (int i = 0; i < file_entry_count; i++) {
        if (strcmp(file_entries[i].full, path) != 0) continue;
        if (is_music(file_entries[i].name)) music_start(&file_entries[i]);
        else show_preview(&file_entries[i]);
        break;
    }
}

void panels_set_modes(bool light, bool night, bool saver) {
    light_on = light;
    night_on = night;
    saver_on = saver;
    if (settings_panel) settings_refresh();
}

void panels_show(int idx) {
    lv_obj_add_flag(radio_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(files_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *p = idx == 0 ? radio_panel : idx == 1 ? files_panel : settings_panel;
    lv_obj_remove_flag(p, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(p);

    if (idx == 1) load_dir(file_cwd[0] ? file_cwd : file_root);
    if (idx == 0) radio_update();
    if (idx == 2) settings_refresh();
}

void panels_hide(void) {
    if (radio_panel) lv_obj_add_flag(radio_panel, LV_OBJ_FLAG_HIDDEN);
    if (files_panel) lv_obj_add_flag(files_panel, LV_OBJ_FLAG_HIDDEN);
    if (settings_panel) lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
}

void panels_set_now(long epoch) {
    time_t t = (time_t)epoch;
    struct tm n;
    localtime_r(&t, &n);
    cal = n;
    cal.tm_isdst = -1;
    cal_ready = true;
    cal_show();
}

void panels_show_reset_confirm(void) {
    reset_ask_cb(NULL);
}

void panels_show_disclaimer(void) {
    disclaimer_open_cb(NULL);
}

void panels_select_storage(int index) {
    if (files_panel) files_select_root(index);
}

void panels_set_files_root(const char *root) {
    if (!root || !root[0]) return;
    const char *roots[1] = {root};
    const char *labels[1] = {"STORAGE"};
    panels_set_storage_roots(roots, labels, 1);
}

void panels_set_storage_roots(const char *const *roots, const char *const *labels, int count) {
    if (!roots || count <= 0) return;
    if (count > FILE_ROOTS_MAX) count = FILE_ROOTS_MAX;
    file_root_count = count;
    file_root_index = 0;
    for (int i = 0; i < count; i++) {
        snprintf(file_roots[i], sizeof(file_roots[i]), "%s", roots[i] ? roots[i] : "");
        snprintf(file_root_labels[i], sizeof(file_root_labels[i]), "%s",
                 (labels && labels[i] && labels[i][0]) ? labels[i] : "STORAGE");
    }
    snprintf(file_root, sizeof(file_root), "%s", file_roots[0]);
    file_cwd[0] = '\0';
    if (files_panel) {
        files_filter_refresh();
        files_source_refresh();
        load_dir(file_root);
    }
}
