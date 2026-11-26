#ifndef STRADA_AUDIO_PLAYER_H
#define STRADA_AUDIO_PLAYER_H

#include <stdbool.h>

/* Start a file. Returns true when real audio output is running (WAV in the
 * SDL emulator). On the framebuffer target, or for codecs the emulator cannot
 * decode, it returns false and the UI runs a simulated transport. */
bool audio_player_play(const char *path, int *duration_sec);
void audio_player_pause(bool pause);
void audio_player_stop(void);

#endif
