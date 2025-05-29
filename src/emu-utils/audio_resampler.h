#ifndef _EMU_UTILS_AUDIO_RESAMPLER_H_
#define _EMU_UTILS_AUDIO_RESAMPLER_H_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef struct audio_resampler_info
{
    uint32_t    dst_sample_rate;
    void*       dst_buffer;
    size_t      dst_buffer_size;
} audio_resampler_info;

typedef struct audio_resampler
{
    audio_resampler_info    info;
    int16_t*                sample_ptr;
    int16_t*                sample_ptr_end;
    float                   sample_step;
    float                   sample_step_adjustment;
    float                   sample_pos;
    float                   high_pass_alpha;
    float                   output_sample;
} audio_resampler;

inline int audio_resampler_init(audio_resampler* resampler, audio_resampler_info* info)
{
    resampler->info                 = *info;
    resampler->sample_ptr           = (int16_t*)info->dst_buffer;
    resampler->sample_ptr_end       = resampler->sample_ptr + (info->dst_buffer_size / 2);
    resampler->sample_pos           = 0.0f;

    resampler->output_sample = 0.0f;
    resampler->sample_step_adjustment = 0.0f;

    return resampler->sample_ptr < resampler->sample_ptr_end;
}

inline void audio_resampler_begin(audio_resampler* resampler, uint32_t src_sample_rate)
{
    // Setup high-pass filter parameters
    float dt = 1.0f / src_sample_rate;
    float RC = 1.0f / (resampler->info.dst_sample_rate * 2 * M_PI);
    resampler->high_pass_alpha = dt / (RC + dt);

    resampler->sample_step = (float)resampler->info.dst_sample_rate / (float)src_sample_rate;
}

inline float interpolate(float a, float b, float t)
{
    return (1.0f - t) * a + t * b;
}

inline int audio_resampler_process_sample(audio_resampler* resampler, int16_t sample, int queue_size)
{
    resampler->output_sample += resampler->high_pass_alpha * (sample - resampler->output_sample);

    resampler->sample_pos += resampler->sample_step + resampler->sample_step_adjustment;

    if (resampler->sample_pos >= 1.0f)
    {
        resampler->sample_pos -= 1.0f;
        *resampler->sample_ptr++ = (int16_t)resampler->output_sample;

        if (resampler->sample_ptr == resampler->sample_ptr_end)
        {
            resampler->sample_ptr = (int16_t*)resampler->info.dst_buffer;

            float target_adjustment = 0.00005f;
            if (queue_size > 6)       target_adjustment = -0.0001f;
            else if (queue_size < 4)  target_adjustment = 0.0001f;
            else                      target_adjustment = 0.00005f;

            float current_adjustment = resampler->sample_step_adjustment;
            float t = 0.05f;

            resampler->sample_step_adjustment =  (1.0f - t)*current_adjustment + t*target_adjustment;

            return 1;
        }
    }

    return 0;
}

inline void audio_resampler_end(audio_resampler* resampler) {}

#endif
