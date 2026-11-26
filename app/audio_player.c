#include "audio_player.h"

#ifdef STRADA_USE_SDL
#include <SDL2/SDL.h>
#endif

static bool available = false;

#ifdef STRADA_USE_SDL
static SDL_AudioDeviceID device = 0;

static bool ensure_audio(void) {
    if (device) return true;
    if (!available) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
        available = true;
    }
    return true;
}

bool audio_player_play(const char *path, int *duration_sec) {
    if (!ensure_audio()) return false;

    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path, &spec, &buf, &len)) return false;

    if (!device) {
        SDL_AudioSpec want = spec;
        device = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
        if (!device) {
            SDL_free(buf);
            return false;
        }
    }

    SDL_ClearQueuedAudio(device);
    SDL_QueueAudio(device, buf, len);
    SDL_PauseAudioDevice(device, 0);

    int frame = (SDL_AUDIO_BITSIZE(spec.format) / 8) * spec.channels;
    if (duration_sec) {
        *duration_sec = (frame > 0 && spec.freq > 0)
                            ? (int)(len / ((Uint32)spec.freq * (Uint32)frame))
                            : 0;
    }
    SDL_free(buf);
    return true;
}

void audio_player_pause(bool pause) {
    if (device) SDL_PauseAudioDevice(device, pause ? 1 : 0);
}

void audio_player_stop(void) {
    if (!device) return;
    SDL_ClearQueuedAudio(device);
    SDL_PauseAudioDevice(device, 1);
}
#else
bool audio_player_play(const char *path, int *duration_sec) {
    (void)path;
    (void)duration_sec;
    return false;
}
void audio_player_pause(bool pause) { (void)pause; }
void audio_player_stop(void) {}
#endif
