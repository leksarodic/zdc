#include "call_panel.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "theme.h"

#define C_CARD    theme_color(0x1E2128)
#define C_INNER   theme_color(0x262A33)
#define C_BTN     theme_color(0x2A2E37)
#define C_TEXT    theme_color(0xF2F3F5)
#define C_MUTED   theme_color(0x8B9099)
#define C_SOFT    theme_color(0xC5C8CE)
#define C_GREEN   0x2DBE6C

static lv_obj_t *panel;
static lv_obj_t *scrim;
static lv_obj_t *display_lbl;
static char dial[40];
static void (*cb_close)(void);
static void (*cb_call)(const char *number);

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
                        uint32_t bg, const lv_font_t *font, lv_event_cb_t cb, void *data) {
    lv_obj_t *b = lv_button_create(parent);
    strip(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, col(bg), 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(C_TEXT), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
    return b;
}

static void update_display(void) {
    lv_label_set_text(display_lbl, dial[0] ? dial : "Enter number");
    lv_obj_set_style_text_color(display_lbl, col(dial[0] ? C_TEXT : C_MUTED), 0);
}

static void close_cb(lv_event_t *e) {
    (void)e;
    call_panel_hide();
    if (cb_close) cb_close();
}

static void digit_cb(lv_event_t *e) {
    char c = (char)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(dial);
    if (n + 1 < sizeof(dial)) {
        dial[n] = c;
        dial[n + 1] = '\0';
        update_display();
    }
}

static void clear_cb(lv_event_t *e) {
    (void)e;
    dial[0] = '\0';
    update_display();
}

static void backspace_cb(lv_event_t *e) {
    (void)e;
    size_t n = strlen(dial);
    if (n) dial[n - 1] = '\0';
    update_display();
}

static void contact_cb(lv_event_t *e) {
    const char *number = (const char *)lv_event_get_user_data(e);
    snprintf(dial, sizeof(dial), "%s", number ? number : "");
    update_display();
}

static void call_cb(lv_event_t *e) {
    (void)e;
    if (!dial[0]) return;
    if (cb_call) cb_call(dial);
    call_panel_hide();
}

void call_panel_init(lv_obj_t *parent, void (*on_close)(void), void (*on_call)(const char *number)) {
    cb_close = on_close;
    cb_call = on_call;

    scrim = box(parent, 0, 0, 800, 480, 0x000000, 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_50, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scrim, close_cb, LV_EVENT_CLICKED, NULL);

    panel = box(parent, 120, 60, 560, 280, C_CARD, 16);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);

    label(panel, "CALL", 18, 12, 200, 18, &lv_font_montserrat_12, C_MUTED, LV_TEXT_ALIGN_LEFT);
    label(panel, "RECENT", 20, 42, 200, 14, &lv_font_montserrat_10, C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(panel, LV_SYMBOL_CLOSE, 522, 10, 24, 24, C_INNER, &lv_font_montserrat_10, close_cb, NULL);

    static const char *CONTACTS[][2] = {
        {"Aleksa", "+381 60 123 4567"},
        {"Home", "+381 11 123 456"},
        {"Work", "+381 11 987 654"},
    };
    for (int i = 0; i < 3; i++) {
        char line[64];
        snprintf(line, sizeof(line), "%s\n%s", CONTACTS[i][0], CONTACTS[i][1]);
        lv_obj_t *b = button(panel, line, 20, 64 + i * 40, 230, 34, C_INNER,
                             &lv_font_montserrat_10, contact_cb, (void *)CONTACTS[i][1]);
        (void)b;
    }

    display_lbl = label(panel, "Enter number", 260, 42, 220, 34, &lv_font_montserrat_18,
                        C_MUTED, LV_TEXT_ALIGN_LEFT);
    button(panel, "DEL", 488, 42, 54, 34, C_INNER, &lv_font_montserrat_10, backspace_cb, NULL);

    static const char DIGITS[4][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'},
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            char s[2] = {DIGITS[r][c], '\0'};
            button(panel, s, 260 + c * 70, 88 + r * 36, 60, 30, C_INNER,
                   &lv_font_montserrat_16, digit_cb, (void *)(intptr_t)DIGITS[r][c]);
        }
    }

    button(panel, "Call", 260, 238, 200, 30, C_GREEN, &lv_font_montserrat_12, call_cb, NULL);
    button(panel, "Clear", 470, 238, 64, 30, C_BTN, &lv_font_montserrat_10, clear_cb, NULL);

    update_display();
}

void call_panel_show(void) {
    if (!panel) return;
    dial[0] = '\0';
    update_display();
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(scrim);
    lv_obj_move_foreground(panel);
}

void call_panel_hide(void) {
    if (scrim) lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    if (panel) lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

bool call_panel_visible(void) {
    return panel && !lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN);
}
