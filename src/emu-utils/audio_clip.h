#ifndef _EMU_UTILS_AUDIO_CLIP_H_
#define _EMU_UTILS_AUDIO_CLIP_H_

#include "../emu/nes_system.h"

typedef struct audio_clip_t audio_clip_t;

audio_clip_t*   audio_clip_create_from_file(const char* filename);
void            audio_clip_destroy(audio_clip_t* clip);

void            audio_clip_begin_playback(audio_clip_t* clip);
void            audio_clip_end_playback(audio_clip_t* clip);
int             audio_clip_is_playing(audio_clip_t* clip);
uint64_t        audio_clip_sample_count(audio_clip_t* clip);
uint32_t        audio_clip_event_count(audio_clip_t* clip);
int16_t         audio_clip_next_sample(audio_clip_t* clip);

int             audio_clip_save_to_file(audio_clip_t* clip, const char* filename);

typedef struct audio_clip_layer_t
{
    nes_system_layer base;
    audio_clip_t*    audio_clip;

} audio_clip_layer_t;

void audio_clip_layer_init(audio_clip_layer_t* layer);
void audio_clip_layer_cleanup(audio_clip_layer_t* layer);
void audio_clip_layer_begin_record(audio_clip_layer_t* layer);
void audio_clip_layer_end_record(audio_clip_layer_t* layer);
int  audio_clip_layer_is_recording(audio_clip_layer_t* layer);

#endif
