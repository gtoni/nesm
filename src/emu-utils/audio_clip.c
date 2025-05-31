#include "audio_clip.h"
#include "../emu/nes_apu.h"
#include <memory.h>
#include <assert.h>
#include <stdio.h>

#define RECORD_VALIDATION 0

typedef struct audio_clip_event_t
{
    uint32_t    sample_index;
    uint16_t    reg_addr;
    uint8_t     reg_data;

} audio_clip_event_t;

typedef struct audio_clip_t
{
#if RECORD_VALIDATION
    int16_t*    samples;
    uint32_t    samples_capacity;
#endif

    nes_apu apu_initial_state;
    nes_apu apu_current_state;

    int                 last_dmc_sample_buffer_loaded;

    audio_clip_event_t* events;
    uint32_t            event_count;
    uint32_t            event_capacity;
    uint32_t            current_event;

    uint64_t            current_sample;
    uint64_t            sample_count;

} audio_clip_t;

void audio_clip_init(audio_clip_t* clip)
{
    memset(clip, 0, sizeof(audio_clip_t));
    clip->current_sample = (uint64_t)-1;
}

void audio_clip_cleanup(audio_clip_t* clip)
{
#if RECORD_VALIDATION
    if (clip->samples)
    {
        free(clip->samples);
        clip->samples = 0;
        clip->samples_capacity = 0;
    }
#endif

    if (clip->events)
    {
        free(clip->events);
        clip->events = 0;
        clip->event_capacity = 0;
    }

    clip->event_count = 0;
    clip->sample_count = 0;
}

void audio_clip_begin_playback(audio_clip_t* clip)
{
    clip->apu_current_state = clip->apu_initial_state;
    clip->current_sample = 0;
    clip->current_event = 0;
}

void audio_clip_end_playback(audio_clip_t* clip)
{
    clip->current_sample = (uint32_t)-1;
}

int audio_clip_is_playing(audio_clip_t* clip)
{
    return clip->current_sample < clip->sample_count;
}

uint64_t audio_clip_sample_count(audio_clip_t *clip)
{
    return clip->sample_count;
}

uint32_t audio_clip_event_count(audio_clip_t *clip)
{
    return clip->event_count;
}

int16_t audio_clip_next_sample(audio_clip_t* clip)
{
    if (clip->current_event < clip->event_count)
    {
        while(1)
        {
            audio_clip_event_t* evt = clip->events + clip->current_event;

            if (evt->sample_index == clip->current_sample)
            {
                if (evt->reg_addr == 0xFFFF)
                {
                    clip->apu_current_state.dmc.sample_buffer_loaded = 1;
                    clip->apu_current_state.dmc.sample_buffer = evt->reg_data;

                    if (--clip->apu_current_state.dmc.bytes_remaining == 0)
                        if (clip->apu_current_state.dmc.loop)
                            clip->apu_current_state.dmc.bytes_remaining = clip->apu_current_state.dmc.sample_length;
                }
                else
                {
                    clip->apu_current_state.reg_rw_mode = NES_APU_REG_RW_MODE_WRITE;
                    clip->apu_current_state.reg_addr = evt->reg_addr;
                    clip->apu_current_state.reg_data = evt->reg_data;
                }
                clip->current_event++;
            }
            else
            {
                break;
            }
        }
    }

    int16_t sample = 0;

    if (clip->apu_current_state.sample_count)
        sample = clip->apu_current_state.samples[clip->apu_current_state.sample_count - 1];

    nes_apu_execute(&clip->apu_current_state);

    if (clip->current_sample < clip->sample_count)
    {
#if RECORD_VALIDATION
        int16_t test_sample = clip->samples[clip->current_sample];

        if (sample != test_sample)
        {
            printf("FAIL sample: %d\n", clip->current_sample);
            clip->sample_count = clip->current_sample + 1;
        }
#endif

        clip->current_sample++;
        return sample;
    }
    return 0;
}

void audio_clip_add_event(audio_clip_t* clip, audio_clip_event_t evt)
{
    if ((clip->event_count + 1) >= clip->event_capacity)
    {
        uint32_t new_capacity = clip->event_capacity * 2;
        if (new_capacity == 0) new_capacity = 1;

        clip->events = (audio_clip_event_t*)realloc(clip->events, sizeof(audio_clip_event_t) * new_capacity);
        clip->event_capacity = new_capacity;
    }

    clip->events[clip->event_count++] = evt;
}

int audio_clip_save_to_file(audio_clip_t* clip, const char* filename)
{
    FILE* f = fopen(filename, "wb");
    if (f)
    {
        fwrite(&clip->apu_initial_state, sizeof(nes_apu), 1, f);
        fwrite(&clip->event_count, sizeof(clip->event_count), 1, f);
        fwrite(&clip->sample_count, sizeof(clip->sample_count), 1, f);
        fwrite(clip->events, sizeof(audio_clip_event_t), clip->event_count, f);
        fclose(f);
        return 1;
    }
    return 0;
}

audio_clip_t* audio_clip_create_from_file(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (f)
    {
        audio_clip_t* clip = (audio_clip_t*)malloc(sizeof(audio_clip_t));
        audio_clip_init(clip);

        fread(&clip->apu_initial_state, sizeof(nes_apu), 1, f);
        fread(&clip->event_count, sizeof(clip->event_count), 1, f);
        fread(&clip->sample_count, sizeof(clip->sample_count), 1, f);

        clip->events = (audio_clip_event_t*)calloc(clip->event_count, sizeof(audio_clip_event_t));
        fread(clip->events, sizeof(audio_clip_event_t), clip->event_count, f);

        clip->event_capacity = clip->event_count;

        fclose(f);

        return clip;
    }
    return 0;
}

void audio_clip_destroy(audio_clip_t *clip)
{
    audio_clip_cleanup(clip);
    free(clip);
}

// Layer

void audio_clip_record_apu(nes_apu* state, void* client_data);

void audio_clip_layer_init(audio_clip_layer_t* layer)
{
    memset(layer, 0, sizeof(audio_clip_layer_t));
    layer->base.client_data = layer;
    layer->audio_clip = (audio_clip_t*)malloc(sizeof(audio_clip_t));
    audio_clip_init(layer->audio_clip);
}

void audio_clip_layer_cleanup(audio_clip_layer_t* layer)
{
    audio_clip_cleanup(layer->audio_clip);
    free(layer->audio_clip);
}

void audio_clip_layer_begin_record(audio_clip_layer_t* layer)
{
    layer->base.apu_callback = audio_clip_record_apu;
    layer->audio_clip->sample_count = 0;
    layer->audio_clip->event_count = 0;
}

void audio_clip_layer_end_record(audio_clip_layer_t* layer)
{
    layer->base.apu_callback = 0;
}

void audio_clip_record_apu(nes_apu* apu, void* client_data)
{
    audio_clip_layer_t* layer = (audio_clip_layer_t*)client_data;
    audio_clip_t* clip = layer->audio_clip;

    if (clip->sample_count == 0)
    {
        clip->last_dmc_sample_buffer_loaded = apu->dmc.sample_buffer_loaded;
        clip->apu_initial_state = *apu;
#if RECORD_VALIDATION
        clip->apu_current_state = clip->apu_initial_state;
#endif
    }

    if (apu->dmc.sample_buffer_loaded != clip->last_dmc_sample_buffer_loaded)
    {
        clip->last_dmc_sample_buffer_loaded = apu->dmc.sample_buffer_loaded;

        if (apu->dmc.sample_buffer_loaded)
        {
            audio_clip_event_t evt;
            evt.sample_index    = clip->sample_count;
            evt.reg_addr        = 0xFFFF;
            evt.reg_data        = apu->dmc.sample_buffer;

            audio_clip_add_event(clip, evt);

#if RECORD_VALIDATION
            clip->apu_current_state.dmc.sample_buffer_loaded = 1;
            clip->apu_current_state.dmc.sample_buffer = evt.reg_data;

            if (--clip->apu_current_state.dmc.bytes_remaining == 0)
                if (clip->apu_current_state.dmc.loop)
                    clip->apu_current_state.dmc.bytes_remaining = clip->apu_current_state.dmc.sample_length;
#endif
        }
    }

    if (apu->reg_rw_mode == NES_APU_REG_RW_MODE_WRITE)
    {
        audio_clip_event_t evt;
        evt.sample_index    = clip->sample_count;
        evt.reg_addr        = apu->reg_addr;
        evt.reg_data        = apu->reg_data;

        audio_clip_add_event(clip, evt);

#if RECORD_VALIDATION
        clip->apu_current_state.reg_rw_mode = NES_APU_REG_RW_MODE_WRITE;
        clip->apu_current_state.reg_addr = evt.reg_addr;
        clip->apu_current_state.reg_data = evt.reg_data;
#endif
    }

#if RECORD_VALIDATION
    if ((clip->sample_count + 1) >= clip->samples_capacity)
    {
        uint32_t new_capacity = clip->samples_capacity * 2;
        if (new_capacity == 0) new_capacity = 1;

        clip->samples = (int16_t*)realloc(clip->samples, sizeof(int16_t) * new_capacity);
        clip->samples_capacity = new_capacity;
    }

    clip->samples[clip->sample_count] = apu->samples[apu->sample_count-1];

    int16_t sampleA = clip->apu_current_state.samples[clip->apu_current_state.sample_count-1];
    int16_t sampleB = apu->samples[apu->sample_count-1];

    if (sampleA != sampleB)
        printf("Record validation failed at sample: %d\n", clip->sample_count);

    nes_apu_execute(&clip->apu_current_state);
#endif

    clip->sample_count++;
}

int audio_clip_layer_is_recording(audio_clip_layer_t* layer)
{
    return layer->base.apu_callback != 0;
}
