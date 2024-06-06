#ifndef _NES_SYSTEM_H_
#define _NES_SYSTEM_H_

#include <stdint.h>
#include <stdlib.h>
#include "nes_cartridge.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct nes_controller_state
{
    uint8_t right   : 1;
    uint8_t left    : 1;
    uint8_t down    : 1;
    uint8_t up      : 1;
    uint8_t start   : 1;
    uint8_t select  : 1;
    uint8_t B       : 1;
    uint8_t A       : 1;
} nes_controller_state;

typedef union nes_pixel
{
    struct
    {
        uint8_t hue        : 4;
        uint8_t brightness : 2;
        uint8_t unused     : 2;
    } component;
    uint8_t value;
} nes_pixel;

#define NES_FRAMEBUFFER_ROW_STRIDE 341

typedef struct nes_video_output
{
    nes_pixel*  framebuffer;
    uint16_t    width;
    uint16_t    height;
    uint8_t     odd_frame       : 1;
    uint8_t     emphasize_red   : 1;
    uint8_t     emphasize_green : 1;
    uint8_t     emphasize_blue  : 1;
} nes_video_output;

typedef struct nes_audio_output
{
    int16_t*    samples;
    uint32_t    sample_count;
    uint32_t    sample_rate;
} nes_audio_output;

typedef enum nes_source_type
{
    NES_SOURCE_FILE,
    NES_SOURCE_MEMORY,
    NES_SOURCE_CARTRIGE
} nes_source_type;

typedef struct nes_source
{
    struct { const void* data; size_t data_size; } memory;
    const char*     file_path;
    nes_cartridge*  cartridge;
} nes_source;

typedef struct nes_config
{
    nes_source_type         source_type;
    nes_source              source;
    void*                   client_data;
    nes_controller_state    (*input_callback)(int controller_id, void* client_data);
    void                    (*video_callback)(const nes_video_output* video_output, void* client_data);
    void                    (*audio_callback)(const nes_audio_output* audio_output, void* client_data);
} nes_config;

typedef struct nes_system nes_system;

nes_system* nes_system_create(nes_config* config);
void        nes_system_destroy(nes_system* system);
void        nes_system_reset(nes_system* system);

size_t      nes_system_get_state_size(nes_system* system);
int         nes_system_save_state(nes_system* system, void* buffer, size_t buffer_size);
int         nes_system_load_state(nes_system* system, const void* buffer, size_t buffer_size);

void        nes_system_tick(nes_system* system);
void        nes_system_frame(nes_system* system);

#if defined(__cplusplus)
}
#endif

#endif
