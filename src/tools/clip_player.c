#include <stdio.h>
#include <stdint.h>
#include <SDL.h>
#include "emu-utils/audio_resampler.h"
#include "emu-utils/audio_clip.h"

#define SAMPLE_RATE 44100

int quit = 0;

SDL_AudioDeviceID   audio_device_id;

audio_resampler     resampler;

void on_sigint(int signum)
{
    quit = 1;
}

int main(int argc, char** argv)
{
    signal(SIGINT, on_sigint);

    const char*     ac_path = "test.ac";
    SDL_AudioSpec   audio_spec_desired, audio_spec_obtained;

    for (int i = 1; i < argc; ++i)
    {
        ac_path = argv[i];
    }

    audio_clip_t* clip = audio_clip_create_from_file(ac_path);
    if (!clip)
    {
        printf("Failed to load audio clip: %s\n", ac_path);
        return -1;
    }

    audio_resampler_info resampler_info;
    resampler_info.dst_buffer_size  = sizeof(int16_t) * 512;
    resampler_info.dst_buffer       = malloc(resampler_info.dst_buffer_size);
    resampler_info.dst_sample_rate  = SAMPLE_RATE;

    audio_resampler_init(&resampler, &resampler_info);

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to initialized SDL2: %s\n", SDL_GetError());
        return -1;
    }

    memset(&audio_spec_desired, 0, sizeof(SDL_AudioSpec));
    audio_spec_desired.freq = SAMPLE_RATE;
    audio_spec_desired.channels = 1;
    audio_spec_desired.format = AUDIO_S16;
    audio_device_id = SDL_OpenAudioDevice(0, 0, &audio_spec_desired, &audio_spec_obtained, 0);
    if (audio_device_id < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device: %s\n", SDL_GetError());
    }

    uint64_t clip_sample_count = audio_clip_sample_count(clip);
    if (clip_sample_count > 0)
    {
        SDL_PauseAudioDevice(audio_device_id, 0);

        printf("Playback audio clip, samples: %llu events: %d\n", clip_sample_count, audio_clip_event_count(clip));
        audio_resampler_begin(&resampler, 1789773);

        audio_clip_begin_playback(clip);
        for (uint64_t i = 0; i < clip_sample_count; ++i)
        {
            if (audio_resampler_process_sample(&resampler, audio_clip_next_sample(clip), 5))
            {
                SDL_QueueAudio(audio_device_id, resampler.info.dst_buffer, resampler.info.dst_buffer_size);

                while (SDL_GetQueuedAudioSize(audio_device_id) > (10 * resampler.info.dst_buffer_size) && !quit)
                    SDL_Delay(1);
            }
        }
        audio_clip_end_playback(clip);

        audio_resampler_end(&resampler);

        while (SDL_GetQueuedAudioSize(audio_device_id) > 0 && !quit)
            SDL_Delay(1);
    }

    if (audio_device_id >= 0)
        SDL_CloseAudioDevice(audio_device_id);

    free(resampler_info.dst_buffer);

    audio_clip_destroy(clip);

    SDL_Quit();
    return 0;
}
