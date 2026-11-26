#ifndef STRADA_BT_SIM_H
#define STRADA_BT_SIM_H

#include <stdbool.h>
#include <stddef.h>

#define BT_LOCAL_NAME "STRADA"
#define BT_DEVICE_NAME "Aleksa's iPhone"
#define BT_DEVICE_MAC "AA:BB:CC:DD:EE:FF"

typedef enum {
    BT_OFF,
    BT_READY,
    BT_PAIRING,
    BT_CONNECTED,
    BT_PLAYING,
    BT_INCOMING,
    BT_ACTIVE,
    BT_ERROR
} bt_state_t;

typedef struct {
    const char *title;
    const char *artist;
    int duration; /* seconds */
} bt_track_t;

typedef struct {
    bt_state_t state;
    int track_index;
    int position;   /* seconds */
    int call_seconds;
    int pair_seconds;
    bool muted;
} bt_model_t;

void bt_sim_init(bt_model_t *m);
void bt_sim_primary(bt_model_t *m);
void bt_sim_secondary(bt_model_t *m);
void bt_sim_next(bt_model_t *m);
void bt_sim_prev(bt_model_t *m);
void bt_sim_tick(bt_model_t *m, int seconds);
void bt_sim_set_state(bt_model_t *m, bt_state_t state);

const bt_track_t *bt_sim_track(const bt_model_t *m);
int bt_sim_track_count(void);
const char *bt_sim_state_name(bt_state_t state);
bool bt_sim_connected(bt_state_t state);

#endif
