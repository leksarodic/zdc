#ifndef STRADA_UI_H
#define STRADA_UI_H

#include "bt_sim.h"

void ui_init(bt_model_t *model);
void ui_update(void);
void ui_screenshot(const char *path);

#endif
