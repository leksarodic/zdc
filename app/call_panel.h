#ifndef STRADA_CALL_PANEL_H
#define STRADA_CALL_PANEL_H

#include <stdbool.h>

#include "lvgl.h"

void call_panel_init(lv_obj_t *parent, void (*on_close)(void), void (*on_call)(const char *number));
void call_panel_show(void);
void call_panel_hide(void);
bool call_panel_visible(void);

#endif
