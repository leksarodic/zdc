#include "theme.h"

typedef struct {
    uint32_t dark;
    uint32_t light;
} theme_pair_t;

/* Every light value is distinct and none collides with a dark value, so the
 * mapping is reversible without tracking which object used which color. */
static const theme_pair_t PAIRS[] = {
    {0x15171C, 0xF3F5F9}, /* screen background */
    {0x1E2128, 0xFDFDFE}, /* card */
    {0x262A33, 0xE7EBF1}, /* inner surface */
    {0x2A2E37, 0xDCE1E9}, /* button */
    {0x0D0E11, 0xF8F9FC}, /* dock */
    {0xF2F3F5, 0x14161B}, /* primary text */
    {0x8B9099, 0x6C727C}, /* muted text */
    {0xC5C8CE, 0x3E434B}, /* soft text */
    {0x23262E, 0xE2E7EF}, /* circle */
    {0x3A3E48, 0xC4C9D3}, /* idle core */
    {0x4A4F5A, 0xA7ADB8}, /* pairing core */
    {0xFFFFFF, 0x1B1E24}, /* icons/text on colored surfaces */
};

#define PAIR_COUNT ((int)(sizeof(PAIRS) / sizeof(PAIRS[0])))

static bool light_mode = false;

bool theme_is_light(void) {
    return light_mode;
}

uint32_t theme_color(uint32_t dark) {
    if (!light_mode) return dark;
    for (int i = 0; i < PAIR_COUNT; i++) {
        if (PAIRS[i].dark == dark) return PAIRS[i].light;
    }
    return dark;
}

static uint32_t map_from(uint32_t value, bool to_light) {
    for (int i = 0; i < PAIR_COUNT; i++) {
        if (to_light && PAIRS[i].dark == value) return PAIRS[i].light;
        if (!to_light && PAIRS[i].light == value) return PAIRS[i].dark;
    }
    return value;
}

static void recolor(lv_obj_t *o, lv_style_selector_t sel, lv_style_prop_t prop,
                    bool to_light) {
    lv_color_t cur = lv_obj_get_style_prop(o, sel, prop).color;
    uint32_t value = lv_color_to_u32(cur) & 0xFFFFFF;
    uint32_t mapped = map_from(value, to_light);
    if (mapped != value) {
        lv_style_value_t v;
        v.color = lv_color_hex(mapped);
        lv_obj_set_local_style_prop(o, prop, v, sel);
    }
}

static lv_obj_tree_walk_res_t walk_cb(lv_obj_t *o, void *ud) {
    bool to_light = *(bool *)ud;
    recolor(o, LV_PART_MAIN, LV_STYLE_BG_COLOR, to_light);
    recolor(o, LV_PART_MAIN, LV_STYLE_TEXT_COLOR, to_light);
    recolor(o, LV_PART_MAIN, LV_STYLE_BORDER_COLOR, to_light);
    recolor(o, LV_PART_INDICATOR, LV_STYLE_BG_COLOR, to_light);
    recolor(o, LV_PART_INDICATOR, LV_STYLE_ARC_COLOR, to_light);
    recolor(o, LV_PART_KNOB, LV_STYLE_BG_COLOR, to_light);
    return LV_OBJ_TREE_WALK_NEXT;
}

void theme_set_light(lv_obj_t *root, bool light) {
    if (light == light_mode) return;
    light_mode = light;
    if (root) lv_obj_tree_walk(root, walk_cb, &light);
}
