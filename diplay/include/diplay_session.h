#ifndef DIPLAY_SESSION_H
#define DIPLAY_SESSION_H

/*
 * DiPlay-style CarPlay session contract for a future C receiver.
 *
 * This header is a design contract only. It is not implemented and is not
 * part of the stock 7010B firmware. A real implementation needs:
 *   - a lawful CarPlay/MFi accessory identity,
 *   - a USB or Wi-Fi transport,
 *   - H.264 decode (CedarX on F1C200s, or another decoder),
 *   - audio/AEC and touch/HID forwarding.
 *
 * Upstream reference: https://github.com/shihabal3amri/DiPlay
 * DiPlay is GPL-3.0/AGPL-3.0; preserve its licenses if code is ever reused.
 * Do not ship extracted accessory keys.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIPLAY_IDLE = 0,
    DIPLAY_CONNECTING,
    DIPLAY_CONNECTED,
    DIPLAY_STREAMING,
    DIPLAY_DISCONNECTING,
    DIPLAY_ERROR
} diplay_state_t;

typedef enum {
    DIPLAY_TRANSPORT_USB = 0,
    DIPLAY_TRANSPORT_WIFI
} diplay_transport_t;

typedef enum {
    DIPLAY_TOUCH_DOWN = 0,
    DIPLAY_TOUCH_MOVE,
    DIPLAY_TOUCH_UP,
    DIPLAY_TOUCH_CANCEL
} diplay_touch_action_t;

typedef struct {
    diplay_transport_t transport;
    /* Requested stream size. The receiver may negotiate a different size. */
    int width;
    int height;
    int fps;
    /* Optional Wi-Fi configuration; ignored for USB. */
    const char *wifi_ssid;
    const char *wifi_password;
    /* Caller-owned opaque pointer passed back to callbacks. */
    void *user;
} diplay_config_t;

typedef struct {
    /* Session lifecycle. */
    void (*on_state)(void *user, diplay_state_t state, const char *detail);

    /* One H.264 access unit / Annex-B frame. The buffer is valid only for
     * the duration of the callback unless the implementation documents a
     * retain/copy policy. */
    void (*on_video)(void *user, const uint8_t *h264, size_t len,
                     uint64_t pts_us, bool keyframe);

    /* Decoded or compressed audio, implementation-defined. */
    void (*on_audio)(void *user, const uint8_t *data, size_t len,
                     uint64_t pts_us, int sample_rate, int channels);

    /* Optional diagnostic event. */
    void (*on_log)(void *user, const char *line);
} diplay_callbacks_t;

/* Create/destroy a session. Returns NULL if the transport cannot be opened. */
void *diplay_session_create(const diplay_config_t *config,
                            const diplay_callbacks_t *callbacks);
void diplay_session_destroy(void *session);

/* Start/stop. start() is asynchronous; completion arrives via on_state(). */
int diplay_session_start(void *session);
void diplay_session_stop(void *session);

/* Input forwarding. Coordinates are in the negotiated video pixel space. */
int diplay_send_touch(void *session, diplay_touch_action_t action,
                      int x, int y, uint32_t pointer_id);
int diplay_send_key(void *session, uint32_t usage, bool pressed);

/* Optional: tell the receiver the local panel geometry, so it can request a
 * matching stream. Returns 0 on success. */
int diplay_set_display(void *session, int width, int height, int fps);

#ifdef __cplusplus
}
#endif

#endif
