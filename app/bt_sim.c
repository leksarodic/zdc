#include "bt_sim.h"

#include <string.h>

static const bt_track_t TRACKS[] = {
    {"Test Song", "Demo Artist", 215},
    {"Blinding Lights", "The Weeknd", 200},
    {"Midnight City", "M83", 243},
    {"Nocturne No. 2", "Chopin", 275},
};

static const int TRACK_COUNT = (int)(sizeof(TRACKS) / sizeof(TRACKS[0]));

void bt_sim_init(bt_model_t *m) {
    memset(m, 0, sizeof(*m));
    m->state = BT_CONNECTED;
    m->track_index = 0;
    m->position = 68;
}

const bt_track_t *bt_sim_track(const bt_model_t *m) {
    return &TRACKS[m->track_index % TRACK_COUNT];
}

int bt_sim_track_count(void) {
    return TRACK_COUNT;
}

bool bt_sim_connected(bt_state_t state) {
    return state == BT_CONNECTED || state == BT_PLAYING || state == BT_INCOMING || state == BT_ACTIVE;
}

const char *bt_sim_state_name(bt_state_t state) {
    switch (state) {
    case BT_OFF: return "off";
    case BT_READY: return "ready";
    case BT_PAIRING: return "pairing";
    case BT_CONNECTED: return "connected";
    case BT_PLAYING: return "playing";
    case BT_INCOMING: return "incoming";
    case BT_ACTIVE: return "active";
    case BT_ERROR: return "error";
    default: return "unknown";
    }
}

void bt_sim_set_state(bt_model_t *m, bt_state_t state) {
    bt_state_t prev = m->state;
    if (state == BT_INCOMING && prev == BT_PLAYING) m->music_interrupted = true;
    else if (state != BT_INCOMING && state != BT_ACTIVE) m->music_interrupted = false;

    m->state = state;
    m->pair_seconds = 0;
    if (state != BT_ACTIVE) m->call_seconds = 0;
    if (state == BT_OFF || state == BT_READY || state == BT_ERROR) {
        m->muted = false;
    }
}

bool bt_sim_music_interrupted(const bt_model_t *m) {
    return m->music_interrupted && (m->state == BT_INCOMING || m->state == BT_ACTIVE);
}

void bt_sim_primary(bt_model_t *m) {
    switch (m->state) {
    case BT_OFF: bt_sim_set_state(m, BT_READY); break;
    case BT_READY: bt_sim_set_state(m, BT_PAIRING); break;
    case BT_PAIRING: bt_sim_set_state(m, BT_READY); break;
    case BT_CONNECTED: bt_sim_set_state(m, BT_PLAYING); break;
    case BT_PLAYING: bt_sim_set_state(m, BT_CONNECTED); break;
    case BT_INCOMING: bt_sim_set_state(m, BT_ACTIVE); break;
    case BT_ACTIVE:
        if (m->music_interrupted) {
            m->music_interrupted = false;
            bt_sim_set_state(m, BT_PLAYING);
        } else {
            bt_sim_set_state(m, BT_CONNECTED);
        }
        break;
    case BT_ERROR: bt_sim_set_state(m, BT_PAIRING); break;
    default: break;
    }
}

void bt_sim_secondary(bt_model_t *m) {
    switch (m->state) {
    case BT_READY: bt_sim_set_state(m, BT_PAIRING); break;
    case BT_CONNECTED:
    case BT_PLAYING: bt_sim_set_state(m, BT_READY); break;
    case BT_INCOMING:
        if (m->music_interrupted) {
            m->music_interrupted = false;
            bt_sim_set_state(m, BT_PLAYING);
        } else {
            bt_sim_set_state(m, BT_CONNECTED);
        }
        break;
    case BT_ACTIVE: m->muted = !m->muted; break;
    case BT_OFF:
    case BT_ERROR: break;
    default: break;
    }
}

void bt_sim_next(bt_model_t *m) {
    if (!bt_sim_connected(m->state)) return;
    m->track_index = (m->track_index + 1) % TRACK_COUNT;
    m->position = 0;
    m->state = BT_PLAYING;
}

void bt_sim_prev(bt_model_t *m) {
    if (!bt_sim_connected(m->state)) return;
    m->track_index = (m->track_index + TRACK_COUNT - 1) % TRACK_COUNT;
    m->position = 0;
    m->state = BT_PLAYING;
}

void bt_sim_tick(bt_model_t *m, int seconds) {
    if (m->state == BT_PAIRING) {
        m->pair_seconds += seconds;
        if (m->pair_seconds >= 2) bt_sim_set_state(m, BT_CONNECTED);
    }
    if (m->state == BT_PLAYING) {
        m->position += seconds;
        const bt_track_t *t = bt_sim_track(m);
        if (m->position >= t->duration) {
            m->track_index = (m->track_index + 1) % TRACK_COUNT;
            m->position = 0;
        }
    }
    if (m->state == BT_ACTIVE) {
        m->call_seconds += seconds;
    }
}
