#ifndef STRADA_PANELS_H
#define STRADA_PANELS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* idx: 0 = Radio, 1 = Files, 2 = Settings */
void panels_init(lv_obj_t *parent,
                 void (*on_home)(void),
                 void (*on_accent)(uint32_t),
                 void (*on_clock)(bool),
                 void (*on_set_time)(long));
void panels_show(int idx);
void panels_hide(void);
void panels_set_files_root(const char *root);
void panels_set_now(long epoch);

#endif
