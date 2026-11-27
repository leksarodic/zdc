#ifndef STRADA_UI_H
#define STRADA_UI_H

#include "bt_sim.h"

void ui_init(bt_model_t *model);
void ui_update(void);
void ui_set_files_root(const char *root);
void ui_set_storage_roots(const char *const *roots, const char *const *labels, int count);
void ui_select_storage(int index);
void ui_show_panel(int idx);
void ui_show_call(void);
void ui_set_theme(bool light);
void ui_set_night_dim(bool on);
void ui_set_screensaver(bool on);
void ui_show_screensaver(void);
void ui_show_volume(void);
void ui_set_volume(int value);
void ui_volume_up(void);
void ui_volume_down(void);
void ui_set_saver_timeout(int seconds);
void ui_set_radio_mute(bool muted);
void ui_open_file(const char *path);
#ifdef STRADA_USE_SDL
void ui_screenshot(const char *path);
#endif

#endif
