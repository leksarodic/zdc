#include "panels.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define C_CARD    0x1E2128
#define C_INNER   0x262A33
#define C_BTN     0x2A2E37
#define C_TEXT    0xF2F3F5
#define C_MUTED   0x8B9099
#define C_SOFT    0xC5C8CE
#define C_GREEN   0x2DBE6C

static lv_obj_t *radio_panel;
static lv_obj_t *files_panel;
static lv_obj_t *settings_panel;

static void (*cb_home)(void);
static void (*cb_accent)(uint32_t);
static void (*cb_clock)(bool);

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

static void panel_header(lv_obj_t *panel, const char *title, const char *right) {
    label(panel, title, 18, 12, 260, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
    if (right && right[0]) {
        label(panel, right, 500, 12, 200, 18, &lv_font_montserrat_12, 0xFE3211, LV_TEXT_ALIGN_RIGHT);
    }
    lv_obj_t *b = button(panel, LV_SYMBOL_CLOSE, 726, 8, 32, 32, C_INNER, &lv_font_montserrat_12, close_cb, NULL);
    (void)b;
}

static lv_obj_t *make_panel(lv_obj_t *parent) {
    lv_obj_t *p = box(parent, 12, 50, 776, 297, C_CARD, 16);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
}

/* ── Radio ───────────────────────────────────────────────────── */

static lv_obj_t *radio_freq_lbl;
static lv_obj_t *radio_status_lbl;
static lv_obj_t *radio_power_lbl;
static lv_obj_t *radio_mute_lbl;
static lv_obj_t *radio_preset_btns[6];

static double radio_freq = 98.1;
static bool radio_on = true;
static bool radio_mute = false;

static const double PRESETS[6] = {87.5, 90.1, 94.3, 98.1, 101.7, 104.9};

static void radio_update(void) {
    char buf[48];
    if (radio_on) snprintf(buf, sizeof(buf), "%.1f MHz", radio_freq);
    else snprintf(buf, sizeof(buf), "OFF");
    lv_label_set_text(radio_freq_lbl, buf);
    lv_obj_set_style_text_color(radio_freq_lbl, col(radio_on ? C_TEXT : C_MUTED), 0);

    if (!radio_on) lv_label_set_text(radio_status_lbl, "Radio off");
    else if (radio_mute) lv_label_set_text(radio_status_lbl, "Muted");
    else lv_label_set_text(radio_status_lbl, "Stereo / 5 bars");
    lv_obj_set_style_text_color(radio_status_lbl, col(radio_on ? C_GREEN : C_MUTED), 0);

    lv_label_set_text(radio_power_lbl, radio_on ? "Power off" : "Power on");
    lv_label_set_text(radio_mute_lbl, radio_mute ? "Unmute" : "Mute");

    for (int i = 0; i < 6; i++) {
        bool selected = radio_on && fabs(radio_freq - PRESETS[i]) < 0.05;
        lv_obj_set_style_border_width(radio_preset_btns[i], selected ? 2 : 0, 0);
        lv_obj_set_style_border_color(radio_preset_btns[i], col(0xFE3211), 0);
    }
}

static void radio_tune(double delta) {
    if (!radio_on) return;
    radio_freq += delta;
    if (radio_freq > 108.0) radio_freq = 87.5;
    if (radio_freq < 87.5) radio_freq = 108.0;
    radio_update();
}

static void radio_tune_down_cb(lv_event_t *e) { (void)e; radio_tune(-0.1); }
static void radio_tune_up_cb(lv_event_t *e) { (void)e; radio_tune(+0.1); }
static void radio_seek_down_cb(lv_event_t *e) { (void)e; radio_tune(-1.0); }
static void radio_seek_up_cb(lv_event_t *e) { (void)e; radio_tune(+1.0); }

static void radio_power_cb(lv_event_t *e) {
    (void)e;
    radio_on = !radio_on;
    radio_update();
}

static void radio_mute_cb(lv_event_t *e) {
    (void)e;
    radio_mute = !radio_mute;
    radio_update();
}

static void radio_preset_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 6) return;
    radio_on = true;
    radio_freq = PRESETS[idx];
    radio_update();
}

static void build_radio(lv_obj_t *parent) {
    radio_panel = make_panel(parent);
    panel_header(radio_panel, "RADIO", "FM / STEREO");

    radio_freq_lbl = label(radio_panel, "98.1 MHz", 0, 38, 776, 60,
                           &lv_font_montserrat_40, C_TEXT, LV_TEXT_ALIGN_CENTER);
    radio_status_lbl = label(radio_panel, "Stereo / 5 bars", 0, 100, 776, 16,
                             &lv_font_montserrat_12, C_GREEN, LV_TEXT_ALIGN_CENTER);

    button(radio_panel, "- 0.1", 40, 128, 130, 40, C_INNER, &lv_font_montserrat_14, radio_tune_down_cb, NULL);
    button(radio_panel, "Seek -", 180, 128, 130, 40, C_BTN, &lv_font_montserrat_14, radio_seek_down_cb, NULL);
    button(radio_panel, "Seek +", 466, 128, 130, 40, C_BTN, &lv_font_montserrat_14, radio_seek_up_cb, NULL);
    button(radio_panel, "+ 0.1", 606, 128, 130, 40, C_INNER, &lv_font_montserrat_14, radio_tune_up_cb, NULL);

    label(radio_panel, "PRESETS", 40, 180, 300, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 6; i++) {
        char b[8];
        snprintf(b, sizeof(b), "%d", i + 1);
        radio_preset_btns[i] = button(radio_panel, b, 40 + i * 120, 200, 104, 32, C_INNER,
                                      &lv_font_montserrat_12, NULL, NULL);
        lv_obj_add_event_cb(radio_preset_btns[i], radio_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    button(radio_panel, "Power off", 40, 248, 150, 34, 0xFE3211, &lv_font_montserrat_12, radio_power_cb, &radio_power_lbl);
    button(radio_panel, "Mute", 586, 248, 150, 34, C_INNER, &lv_font_montserrat_12, radio_mute_cb, &radio_mute_lbl);

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

static char file_root[STRADA_PATH_MAX] = "media";
static char file_cwd[STRADA_PATH_MAX] = "";
static file_entry_t file_entries[FILE_MAX];
static int file_entry_count = 0;

static lv_obj_t *files_path_lbl;
static lv_obj_t *files_info_lbl;
static lv_obj_t *files_list;
static lv_obj_t *file_preview;
static lv_obj_t *file_preview_title;
static lv_obj_t *file_preview_text;

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
    }
    lv_obj_remove_flag(file_preview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(file_preview);
    lv_label_set_text(file_preview_title, ent->name);

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

static void load_dir(const char *path);

static void file_row_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= file_entry_count) return;
    file_entry_t *ent = &file_entries[idx];
    if (ent->is_dir) {
        load_dir(ent->full);
    } else {
        show_preview(ent);
    }
}

static void load_dir(const char *path) {
    file_entry_count = 0;
    DIR *d = opendir(path);
    if (!d) {
        lv_label_set_text_fmt(files_path_lbl, "Cannot open: %s", path);
        lv_obj_clean(files_list);
        label(files_list, "Directory not available.", 8, 8, 700, 18,
              &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
        lv_label_set_text(files_info_lbl, "0 items");
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && file_entry_count < FILE_MAX) {
        if (de->d_name[0] == '.') continue;
        file_entry_t *e = &file_entries[file_entry_count];
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        snprintf(e->full, sizeof(e->full), "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(e->full, &st) != 0) continue;
        e->is_dir = S_ISDIR(st.st_mode);
        e->size = (long)st.st_size;
        file_entry_count++;
    }
    closedir(d);

    qsort(file_entries, (size_t)file_entry_count, sizeof(file_entry_t), file_cmp);
    snprintf(file_cwd, sizeof(file_cwd), "%s", path);
    lv_label_set_text(files_path_lbl, file_cwd);

    lv_obj_clean(files_list);
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

        lv_obj_t *l = lv_label_create(row);
        if (e->is_dir) {
            lv_label_set_text_fmt(l, "%s  %s", LV_SYMBOL_DIRECTORY, e->name);
        } else {
            lv_label_set_text_fmt(l, "%s  %s   %ld B", LV_SYMBOL_FILE, e->name, e->size);
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

static void build_files(lv_obj_t *parent) {
    files_panel = make_panel(parent);
    panel_header(files_panel, "FILES", "LOCAL / USB");

    files_path_lbl = label(files_panel, file_root, 18, 38, 700, 16,
                           &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(files_panel, LV_SYMBOL_UP, 18, 58, 54, 30, C_INNER, &lv_font_montserrat_12, files_up_cb, NULL);
    button(files_panel, LV_SYMBOL_REFRESH, 80, 58, 54, 30, C_INNER, &lv_font_montserrat_12, files_refresh_cb, NULL);

    files_list = box(files_panel, 18, 94, 740, 158, C_CARD, 8);
    lv_obj_set_style_bg_opa(files_list, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(files_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(files_list, 4, 0);
    lv_obj_set_style_pad_all(files_list, 2, 0);
    lv_obj_set_scroll_dir(files_list, LV_DIR_VER);

    files_info_lbl = label(files_panel, "", 18, 258, 740, 18,
                           &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
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

static void settings_refresh(void) {
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_border_width(accent_btns[i], i == accent_idx ? 3 : 0, 0);
        lv_obj_set_style_border_color(accent_btns[i], col(C_TEXT), 0);
    }
    lv_obj_set_style_bg_color(clock24_lbl ? lv_obj_get_parent(clock24_lbl) : NULL,
                              col(clock_is_24 ? C_BTN : C_INNER), 0);
    lv_obj_set_style_bg_color(clock12_lbl ? lv_obj_get_parent(clock12_lbl) : NULL,
                              col(clock_is_24 ? C_INNER : C_BTN), 0);
    if (volume_slider) {
        int v = lv_slider_get_value(volume_slider);
        lv_label_set_text_fmt(volume_lbl, "%d%%", v);
    }
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
}

static void settings_reset_cb(lv_event_t *e) {
    (void)e;
    accent_idx = 0;
    clock_is_24 = true;
    lv_slider_set_value(volume_slider, 70, LV_ANIM_OFF);
    if (cb_accent) cb_accent(ACCENTS[0]);
    if (cb_clock) cb_clock(true);
    settings_refresh();
}

static void build_settings(lv_obj_t *parent) {
    settings_panel = make_panel(parent);
    panel_header(settings_panel, "SETTINGS", "STRADA 7010B");

    label(settings_panel, "ACCENT", 24, 48, 120, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = box(settings_panel, 120 + i * 50, 42, 34, 34, ACCENTS[i], LV_RADIUS_CIRCLE);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        accent_btns[i] = b;
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, accent_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    label(settings_panel, "CLOCK", 24, 96, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *b24 = button(settings_panel, "24 h", 120, 90, 76, 30, C_BTN, &lv_font_montserrat_12, clock24_cb, &clock24_lbl);
    lv_obj_t *b12 = button(settings_panel, "12 h", 204, 90, 76, 30, C_INNER, &lv_font_montserrat_12, clock12_cb, &clock12_lbl);
    (void)b24; (void)b12;

    label(settings_panel, "VOLUME", 24, 140, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    volume_slider = lv_slider_create(settings_panel);
    lv_obj_set_pos(volume_slider, 120, 136);
    lv_obj_set_size(volume_slider, 300, 20);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 70, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, col(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[0]), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, col(ACCENTS[0]), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider, volume_cb, LV_EVENT_VALUE_CHANGED, NULL);
    volume_lbl = label(settings_panel, "70%", 436, 136, 60, 20,
                       &lv_font_montserrat_12, C_TEXT, LV_TEXT_ALIGN_LEFT);

    label(settings_panel, "ABOUT", 24, 180, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "Bluetooth name", 24, 200, 130, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "STRADA", 160, 200, 160, 14, &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "UI", 24, 216, 60, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "LVGL 9.3 / 800x480", 160, 216, 240, 14, &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "BT link", 24, 232, 60, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "55 AA LEN CMD DATA CHK", 160, 232, 300, 14, &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "Hardware", 24, 248, 80, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(settings_panel, "7010B / F1C200s class", 160, 248, 280, 14, &lv_font_montserrat_10, C_TEXT, LV_TEXT_ALIGN_LEFT);

    button(settings_panel, "Reset UI", 606, 240, 140, 34, C_INNER, &lv_font_montserrat_12, settings_reset_cb, NULL);

    settings_refresh();
}

/* ── public API ──────────────────────────────────────────────── */

void panels_init(lv_obj_t *parent, void (*on_home)(void), void (*on_accent)(uint32_t), void (*on_clock)(bool)) {
    cb_home = on_home;
    cb_accent = on_accent;
    cb_clock = on_clock;

    build_radio(parent);
    build_files(parent);
    build_settings(parent);
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

void panels_set_files_root(const char *root) {
    if (!root || !root[0]) return;
    snprintf(file_root, sizeof(file_root), "%s", root);
    if (files_panel) load_dir(file_root);
}
