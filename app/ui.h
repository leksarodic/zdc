#ifndef STRADA_UI_H
#define STRADA_UI_H

#include "bt_sim.h"

void ui_init(bt_model_t *model);
void ui_update(void);
void ui_set_files_root(const char *root);
void ui_show_panel(int idx);
void ui_show_call(void);
void ui_screenshot(const char *path);

#endif
