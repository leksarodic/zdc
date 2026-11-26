#ifndef STRADA_THEME_H
#define STRADA_THEME_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* Map a dark-palette color to the active palette. Dark colors are the
 * canonical values used across the UI; in light mode they resolve to their
 * daytime counterparts. */
uint32_t theme_color(uint32_t dark);

bool theme_is_light(void);

/* Recolor every object under root for the requested mode and remember it. */
void theme_set_light(lv_obj_t *root, bool light);

#endif
