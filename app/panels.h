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
                 void (*on_set_time)(long),
                 void (*on_volume)(int),
                 void (*on_theme)(bool),
                 void (*on_night)(bool),
                 void (*on_saver)(bool),
                 void (*on_radio_start)(void),
                 void (*on_volume_mute)(bool),
                 void (*on_saver_timeout)(int));
void panels_show(int idx);
void panels_hide(void);
void panels_set_files_root(const char *root);
void panels_set_storage_roots(const char *const *roots, const char *const *labels, int count);
void panels_select_storage(int index);
void panels_show_reset_confirm(void);
void panels_show_disclaimer(void);
void panels_set_now(long epoch);
void panels_set_boot_volume(int value);
void panels_set_saver_timeout(int seconds);
void panels_set_radio_mute(bool muted);
void panels_set_modes(bool light, bool night, bool saver);
void panels_open_file(const char *path);

#endif
