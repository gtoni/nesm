#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include <assert.h>
#include "emu/nes_system.h"
#include "emu-utils/audio_resampler.h"

#define TEXTURE_WIDTH   256
#define TEXTURE_HEIGHT  224

#define CONTROLLER_DEADZONE 2048

#define SAMPLE_RATE 44100

SDL_Rect            video_srcrect = {0,0,TEXTURE_WIDTH, TEXTURE_HEIGHT};
uint32_t*           texture_buffer = 0;
uint32_t            wnd_scale = 3;
SDL_Window*         wnd = 0;
SDL_Renderer*       renderer = 0;
SDL_Texture*        texture = 0;
SDL_GameController* controller[2];
SDL_AudioDeviceID   audio_device_id;

audio_resampler     resampler;

char                save_path[1024];
void*               state_buffer = 0;
size_t              state_buffer_size = 0;

uint32_t            palette_colors[64 * 8];

void init_palette(const char* palette_path)
{
    const uint32_t default_palette[64 * 8] = {
        0xFF545454, 0xFF712000, 0xFF900F11, 0xFF8D042E, 0xFF680049, 0xFF310158, 0xFF010856, 0xFF001643, 0xFF002827, 0xFF00380B, 0xFF004000, 0xFF083D00, 0xFF3C3100, 0xFF000000, 0xFF030303, 0xFF030303,
        0xFF9D9D9D, 0xFFC84E12, 0xFFF53435, 0xFFF01F63, 0xFFBA168C, 0xFF6819A3, 0xFF1A27A0, 0xFF003F84, 0xFF005B58, 0xFF00722C, 0xFF007E0D, 0xFF287B00, 0xFF796901, 0xFF030303, 0xFF030303, 0xFF030303,
        0xFFF5F5F5, 0xFFFF9E55, 0xFFFF7E81, 0xFFFF66B5, 0xFFFF5AE3, 0xFFBB5EFD, 0xFF5F6FF9, 0xFF1C8CDA, 0xFF01ACA9, 0xFF04C675, 0xFF27D44D, 0xFF70D039, 0xFFCEBC3C, 0xFF3F3F3F, 0xFF030303, 0xFF030303,
        0xFFF5F5F5, 0xFFFFD0B0, 0xFFFFC2C3, 0xFFFFB7DA, 0xFFFFB2EE, 0xFFDDB4F8, 0xFFB4BCF7, 0xFF93C8EA, 0xFF82D6D5, 0xFF84E2BF, 0xFF99E7AC, 0xFFBCE6A2, 0xFFE5DDA3, 0xFFA5A5A5, 0xFF030303, 0xFF030303,
        0xFF313555, 0xFF480B00, 0xFF630110, 0xFF63002C, 0xFF480046, 0xFF1E0056, 0xFF000257, 0xFF000B44, 0xFF001728, 0xFF00210D, 0xFF002500, 0xFF002100, 0xFF1C1600, 0xFF000000, 0xFF000003, 0xFF000003,
        0xFF666C9E, 0xFF882B13, 0xFFB11935, 0xFFB10C60, 0xFF880888, 0xFF490CA0, 0xFF0C1AA1, 0xFF002A84, 0xFF003E5B, 0xFF004E30, 0xFF005411, 0xFF0A4E03, 0xFF453C02, 0xFF000003, 0xFF000003, 0xFF000003,
        0xFFA7B0F7, 0xFFCD6657, 0xFFF95081, 0xFFF940B2, 0xFFCD3ADE, 0xFF8740F9, 0xFF3F52FA, 0xFF0866DB, 0xFF007CAC, 0xFF008E7B, 0xFF089553, 0xFF3C8E3E, 0xFF837A3D, 0xFF21253F, 0xFF000003, 0xFF000003,
        0xFFA7B0F7, 0xFFB791B1, 0xFFC887C5, 0xFFC880DA, 0xFFB77DED, 0xFF9A80F8, 0xFF7A88F8, 0xFF5E91EB, 0xFF4E9AD8, 0xFF4FA2C2, 0xFF5EA5B0, 0xFF78A2A6, 0xFF989AA5, 0xFF6C72A6, 0xFF000003, 0xFF000003,
        0xFF224C2C, 0xFF421B00, 0xFF580B00, 0xFF530110, 0xFF330022, 0xFF0D0031, 0xFF000532, 0xFF001325, 0xFF002411, 0xFF003400, 0xFF003D00, 0xFF003900, 0xFF1B2C00, 0xFF000000, 0xFF000100, 0xFF000100,
        0xFF4F915F, 0xFF7F4600, 0xFFA02C13, 0xFF981833, 0xFF690E50, 0xFF2D1366, 0xFF002267, 0xFF003954, 0xFF005434, 0xFF006C15, 0xFF007A00, 0xFF097400, 0xFF446100, 0xFF000100, 0xFF000100, 0xFF000100,
        0xFF86E49D, 0xFFBC9127, 0xFFDF7246, 0xFFD65A6C, 0xFFA44C8D, 0xFF6053A5, 0xFF1E65A6, 0xFF008191, 0xFF00A16D, 0xFF00BC47, 0xFF03CB2B, 0xFF34C418, 0xFF7AAF17, 0xFF16381E, 0xFF000100, 0xFF000100,
        0xFF86E49D, 0xFF9CC169, 0xFFAAB478, 0xFFA6A888, 0xFF92A296, 0xFF77A5A0, 0xFF59AEA1, 0xFF41BA98, 0xFF35C889, 0xFF38D378, 0xFF49DA6B, 0xFF63D762, 0xFF81CE61, 0xFF549965, 0xFF000100, 0xFF000100,
        0xFF1C3232, 0xFF370A00, 0xFF4C0101, 0xFF490012, 0xFF310024, 0xFF0B0033, 0xFF000033, 0xFF000926, 0xFF001413, 0xFF001F02, 0xFF002400, 0xFF002000, 0xFF161500, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF456867, 0xFF6F2903, 0xFF8D1819, 0xFF8A0B37, 0xFF660553, 0xFF2A0969, 0xFF00176A, 0xFF002756, 0xFF003A38, 0xFF004A1A, 0xFF005305, 0xFF034C00, 0xFF3B3B00, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF79AAA9, 0xFFA76331, 0xFFC84D4F, 0xFFC43D73, 0xFF9D3592, 0xFF5A3BAB, 0xFF194DAC, 0xFF006097, 0xFF007774, 0xFF008950, 0xFF009334, 0xFF298C21, 0xFF6D7820, 0xFF112222, 0xFF000000, 0xFF000000,
        0xFF79AAA9, 0xFF8B8C75, 0xFF988283, 0xFF977B92, 0xFF8777A0, 0xFF6C7AAA, 0xFF4E82AB, 0xFF398BA2, 0xFF2F9493, 0xFF309C83, 0xFF3DA076, 0xFF569D6D, 0xFF74956C, 0xFF4A6E6D, 0xFF000000, 0xFF000000,
        0xFF6A3839, 0xFF7A1400, 0xFF98090A, 0xFF920020, 0xFF6F0034, 0xFF3B003E, 0xFF0B003A, 0xFF000628, 0xFF001110, 0xFF001F00, 0xFF002700, 0xFF152800, 0xFF482000, 0xFF000000, 0xFF090000, 0xFF090000,
        0xFFBE7272, 0xFFD63A0A, 0xFFFF2729, 0xFFF9144C, 0xFFC50A6B, 0xFF790A7A, 0xFF2D1274, 0xFF002259, 0xFF003433, 0xFF004A14, 0xFF055800, 0xFF3D5800, 0xFF8C4D00, 0xFF090000, 0xFF090000, 0xFF090000,
        0xFFFFB8B9, 0xFFFF7A3E, 0xFFFF6466, 0xFFFF4C8E, 0xFFFF3FB1, 0xFFD73EC2, 0xFF8049BB, 0xFF3D5D9E, 0xFF1C7372, 0xFF228C4B, 0xFF4B9B2E, 0xFF939C21, 0xFFED8F27, 0xFF512828, 0xFF090000, 0xFF090000,
        0xFFFFB8B9, 0xFFFF9E84, 0xFFFF9496, 0xFFFF89A7, 0xFFFF83B6, 0xFFFF83BD, 0xFFDE88BA, 0xFFBE91AE, 0xFFAC9B9B, 0xFFB0A68A, 0xFFC5AC7C, 0xFFE6AC76, 0xFFFFA778, 0xFFC87879, 0xFF090000, 0xFF090000,
        0xFF3D2938, 0xFF4C0800, 0xFF68000A, 0xFF65001D, 0xFF4B0030, 0xFF25003B, 0xFF02003A, 0xFF000228, 0xFF000D10, 0xFF001600, 0xFF001B00, 0xFF001A00, 0xFF1F1200, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF7A5971, 0xFF8F2509, 0xFFB91428, 0xFFB50749, 0xFF8F0266, 0xFF540476, 0xFF1B0C74, 0xFF001B59, 0xFF002D33, 0xFF003C15, 0xFF004402, 0xFF144200, 0xFF4C3600, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFC495B8, 0xFFDB5A3D, 0xFFFF4565, 0xFFFF348A, 0xFFDB2DAB, 0xFF9A2FBD, 0xFF573BBA, 0xFF1B4D9D, 0xFF016371, 0xFF03754D, 0xFF1B7E31, 0xFF4E7B23, 0xFF906E25, 0xFF2C1B27, 0xFF000000, 0xFF000000,
        0xFFC495B8, 0xFFCD7C82, 0xFFDF7394, 0xFFDE6BA5, 0xFFCD67B2, 0xFFB268BA, 0xFF956EB9, 0xFF7876AC, 0xFF68809A, 0xFF69888A, 0xFF788B7D, 0xFF918A76, 0xFFAE8577, 0xFF815F78, 0xFF000000, 0xFF000000,
        0xFF363425, 0xFF4A1100, 0xFF5F0600, 0xFF5A000E, 0xFF3A0020, 0xFF16002A, 0xFF000029, 0xFF00051D, 0xFF000F0B, 0xFF001D00, 0xFF002600, 0xFF032400, 0xFF261C00, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF6E6B53, 0xFF8C3400, 0xFFAC2210, 0xFFA30F2F, 0xFF74064C, 0xFF3C075B, 0xFF091059, 0xFF002046, 0xFF00322A, 0xFF00480C, 0xFF005500, 0xFF1C5300, 0xFF554600, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFB3AE8C, 0xFFD47122, 0xFFF65B3F, 0xFFED4364, 0xFFBA3784, 0xFF7B3996, 0xFF3B4593, 0xFF0F587E, 0xFF006E5D, 0xFF018839, 0xFF20961E, 0xFF549411, 0xFF978613, 0xFF252418, 0xFF000000, 0xFF000000,
        0xFFB3AE8C, 0xFFC0945E, 0xFFCE8A6B, 0xFFCA807B, 0xFFB67A89, 0xFF9B7B90, 0xFF7F818F, 0xFF688A87, 0xFF5C9379, 0xFF5F9E68, 0xFF71A45B, 0xFF8AA355, 0xFFA79D56, 0xFF747158, 0xFF000000, 0xFF000000,
        0xFF282828, 0xFF3C0800, 0xFF500000, 0xFF4E0010, 0xFF360021, 0xFF12002B, 0xFF00002A, 0xFF00021E, 0xFF000D0C, 0xFF001600, 0xFF001B00, 0xFF001A00, 0xFF191200, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF595959, 0xFF762500, 0xFF951415, 0xFF910732, 0xFF6D024E, 0xFF36045D, 0xFF040C5B, 0xFF001B48, 0xFF002D2B, 0xFF003C0F, 0xFF004400, 0xFF0C4200, 0xFF413600, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF959595, 0xFFB55929, 0xFFD74446, 0xFFD33469, 0xFFAB2D89, 0xFF6D2F9A, 0xFF303B98, 0xFF064D82, 0xFF006361, 0xFF00753F, 0xFF0C7E24, 0xFF3B7B17, 0xFF7A6E19, 0xFF1B1B1B, 0xFF000000, 0xFF000000,
        0xFF959595, 0xFFA27C66, 0xFFAF7273, 0xFFAE6B83, 0xFF9E6790, 0xFF846897, 0xFF696E96, 0xFF53768D, 0xFF47807F, 0xFF488870, 0xFF568B63, 0xFF6E8A5D, 0xFF8A855D, 0xFF5E5E5E, 0xFF000000, 0xFF000000 
    };

    memcpy(palette_colors, default_palette, sizeof(default_palette));

    if (palette_path)
    {
        FILE* file = fopen(palette_path, "rb");
        if (file)
        {
            fseek(file, 0, SEEK_END);
            size_t palette_size = ftell(file) / 3;
            fseek(file, 0, SEEK_SET);

            if (palette_size > 64 * 8)
                palette_size = 64 * 8;

            for (uint32_t i = 0; i < palette_size; ++i)
            {
                fread(palette_colors + i, 3, 1, file);
                palette_colors[i] |= 0xFF000000;
            }

            fclose(file);
        }
        else
        {
            printf("Failed to read palette file: %s\n", palette_path);
        }
    }
}

void write_save()
{
    if (state_buffer)
    {
        FILE* file = fopen(save_path, "wb");
        if (file)
        {
            fwrite(state_buffer, 1, state_buffer_size, file);
            fclose(file);
        }
    }
}

void read_save()
{
    if (!state_buffer)
    {
        FILE* file = fopen(save_path, "rb");
        if (file)
        {
            fseek(file, 0, SEEK_END);
            state_buffer_size = ftell(file);
            state_buffer = malloc(state_buffer_size);

            fseek(file, 0, SEEK_SET);

            fread(state_buffer, 1, state_buffer_size, file);
            fclose(file);
        }
    }
}

nes_controller_state on_nes_input(int controller_id, void* client)
{
    nes_controller_state state;
    memset(&state, 0, sizeof(nes_controller_state));

    if (controller_id == 0)
    {
        const uint8_t* keys = SDL_GetKeyboardState(0);
        int is_ctrl_down = keys[SDL_SCANCODE_LCTRL] | keys[SDL_SCANCODE_RCTRL];
        if (!is_ctrl_down)
        {
            state.up    = keys[SDL_SCANCODE_W];
            state.down  = keys[SDL_SCANCODE_S];
            state.left  = keys[SDL_SCANCODE_A];
            state.right = keys[SDL_SCANCODE_D];
            state.A     = keys[SDL_SCANCODE_K];
            state.B     = keys[SDL_SCANCODE_J];
            state.select = keys[SDL_SCANCODE_TAB];
            state.start  = keys[SDL_SCANCODE_RETURN];
        }

        if (controller[0])
        {
            state.up     |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_DPAD_UP); 
            state.down   |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_DPAD_DOWN); 
            state.left   |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_DPAD_LEFT); 
            state.right  |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_DPAD_RIGHT); 
            state.A      |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_A); 
            state.B      |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_B); 
            state.select |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_BACK); 
            state.start  |= SDL_GameControllerGetButton(controller[0], SDL_CONTROLLER_BUTTON_START); 

            state.left  |= SDL_GameControllerGetAxis(controller[0], SDL_CONTROLLER_AXIS_LEFTX) < -CONTROLLER_DEADZONE;
            state.right |= SDL_GameControllerGetAxis(controller[0], SDL_CONTROLLER_AXIS_LEFTX) > CONTROLLER_DEADZONE;
            state.up    |= SDL_GameControllerGetAxis(controller[0], SDL_CONTROLLER_AXIS_LEFTY) < -CONTROLLER_DEADZONE;
            state.down  |= SDL_GameControllerGetAxis(controller[0], SDL_CONTROLLER_AXIS_LEFTY) > CONTROLLER_DEADZONE;
        }
    }

    return state;
}

void on_nes_video(const nes_video_output* video, void* client)
{
    nes_pixel* pixel = video->framebuffer;
    for (int y = 0; y < video->height; ++y)
    {
        for (int x = 0; x < video->width; ++x)
        {
            uint8_t color_index   = pixel[x].value & 0x3F;
            uint8_t palette_index = pixel[x].value >> 6;
            texture_buffer[y * 256 + x] = palette_colors[palette_index * 64 + color_index];
        }
        pixel += NES_FRAMEBUFFER_ROW_STRIDE;
    }

    SDL_UpdateTexture(texture, 0, texture_buffer, sizeof(uint32_t) * TEXTURE_WIDTH);
    video_srcrect.w = video->width;
    video_srcrect.h = video->height;
}

void on_nes_audio(const nes_audio_output* audio, void* client)
{
    int queue_size = SDL_GetQueuedAudioSize(audio_device_id) / resampler.info.dst_buffer_size;

    audio_resampler_begin(&resampler, audio->sample_rate);

    for (uint32_t i = 0; i < audio->sample_count; ++i)
    {
        if (audio_resampler_process_sample(&resampler, audio->samples[i], queue_size))
        {
            SDL_QueueAudio(audio_device_id, resampler.info.dst_buffer, resampler.info.dst_buffer_size);
            queue_size++;
        }
    }

    audio_resampler_end(&resampler);
}

void handle_shortcut_key(nes_system* system, SDL_Scancode key)
{
    const uint8_t* keys = SDL_GetKeyboardState(0);
    int is_ctrl_down = keys[SDL_SCANCODE_LCTRL] | keys[SDL_SCANCODE_RCTRL];
    if (is_ctrl_down)
    {
        int scale = wnd_scale;
        if (key == SDL_SCANCODE_EQUALS) wnd_scale++;
        else if(key == SDL_SCANCODE_MINUS) wnd_scale--;

        if (scale != wnd_scale)
        {
            SDL_SetWindowSize(wnd, TEXTURE_WIDTH * wnd_scale, TEXTURE_HEIGHT * wnd_scale);
        }

        if (key == SDL_SCANCODE_S)
        {
            if (!state_buffer)
            {
                state_buffer_size   = nes_system_get_state_size(system);
                state_buffer        = malloc(state_buffer_size);
            }

            if (!nes_system_save_state(system, state_buffer, state_buffer_size))
                fprintf(stderr, "Save state failed\n");

            write_save();
        }
        else if (key == SDL_SCANCODE_L)
        {
            read_save();

            if (state_buffer)
            {
                if (!nes_system_load_state(system, state_buffer, state_buffer_size))
                    fprintf(stderr, "Load state failed\n");
            }
        }
        else if (key == SDL_SCANCODE_R)
        {
            nes_system_reset(system);
        }
    }
}

void handle_joystick_added(uint32_t index)
{
    if (index < 2 && !controller[index] && SDL_IsGameController(index))
    {
        controller[index] = SDL_GameControllerOpen(index);
    }
}

void handle_joystick_removed(uint32_t index)
{
    if (index < 2 && controller[index])
    {
        SDL_GameControllerClose(controller[index]);
        controller[index] = 0;
    }
}

int main(int argc, char** argv)
{
    const char*     pal_path = 0;
    const char*     rom_path = "rom.nes";
    char            title[256];
    int             quit = 0;
    nes_config      config;
    nes_system*     system = 0;
    SDL_AudioSpec   audio_spec_desired, audio_spec_obtained;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-pal") == 0 && ++i < argc)
            pal_path = argv[i];
        else
            rom_path = argv[i];
    }

    init_palette(pal_path);

    audio_resampler_info resampler_info;
    resampler_info.dst_buffer_size  = sizeof(int16_t) * 441;
    resampler_info.dst_buffer       = malloc(resampler_info.dst_buffer_size);
    resampler_info.dst_sample_rate  = SAMPLE_RATE;

    audio_resampler_init(&resampler, &resampler_info);

    snprintf(title, 256, "NESM - %s", rom_path);
    snprintf(save_path, 1024, "%s_sav", rom_path);

    config.source_type = NES_SOURCE_FILE;
    config.source.file_path = rom_path;
    config.client_data = 0;
    config.layer = 0;
    config.input_callback = &on_nes_input;
    config.video_callback = &on_nes_video;
    config.audio_callback = &on_nes_audio;

    system = nes_system_create(&config);
    if (!system)
    {
        fprintf(stderr, "Failed to initialized NES system.\n");
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to initialized SDL2: %s\n", SDL_GetError());
        return -1;
    }

    wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            TEXTURE_WIDTH * wnd_scale, TEXTURE_HEIGHT * wnd_scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!wnd)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to create window: %s\n", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(wnd, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to create renderer: %s\n", SDL_GetError());
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

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, TEXTURE_WIDTH, TEXTURE_HEIGHT);
    if (!texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to create texture: %s\n", SDL_GetError());
        return -1;
    }

    texture_buffer = (uint32_t*)malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(uint32_t));

    memset(&controller, 0, 2 * sizeof(SDL_GameController*));

    SDL_ShowWindow(wnd);

    memset(resampler.info.dst_buffer, 0, resampler.info.dst_buffer_size);
    for (uint32_t i = 0; i < 20; ++i)
        SDL_QueueAudio(audio_device_id, resampler.info.dst_buffer, resampler.info.dst_buffer_size);

    SDL_PauseAudioDevice(audio_device_id, 0);

    while(!quit)
    {
        uint64_t begin_frame = SDL_GetPerformanceCounter();

        int w, h, has_key_up = 0;
        SDL_Scancode key_up_scancode = (SDL_Scancode)0;
        SDL_Rect dstrect;
        SDL_Event evt;
        float aspect_ratio = (float)video_srcrect.w / (float)video_srcrect.h;

        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT) quit = 1;
            else if (evt.type == SDL_KEYUP)
            {
                has_key_up = 1;
                key_up_scancode = evt.key.keysym.scancode;
            }
            else if (evt.type == SDL_JOYDEVICEADDED)
            {
                handle_joystick_added(evt.jdevice.which);
            }
            else if (evt.type == SDL_JOYDEVICEREMOVED)
            {
                handle_joystick_removed(evt.jdevice.which);
            }
        }

        if (has_key_up)
            handle_shortcut_key(system, key_up_scancode);

        SDL_GetWindowSize(wnd, &w, &h);
        if (((float)w / (float)h) >= aspect_ratio)
        {
            dstrect.y = 0;
            dstrect.h = h;
            dstrect.w = (int)((float)h * aspect_ratio);
            dstrect.x = (w - dstrect.w)>>1;
        }
        else
        {
            dstrect.x = 0;
            dstrect.w = w;
            dstrect.h = (int)((float)w / aspect_ratio);
            dstrect.y = (h - dstrect.h)>>1;
        }

        nes_system_frame(system);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &video_srcrect, &dstrect);
        SDL_RenderPresent(renderer);

        uint64_t frame_time = 0;
        while(frame_time < 1666)
        {
           frame_time  = ((SDL_GetPerformanceCounter() - begin_frame)*100000)/SDL_GetPerformanceFrequency();
           if (frame_time < 1666 && (1666 - frame_time) > 1000)
               SDL_Delay(1);
        }
    }

    SDL_DestroyWindow(wnd);
    if (audio_device_id >= 0)
        SDL_CloseAudioDevice(audio_device_id);

    nes_system_destroy(system);
    free(texture_buffer);

    free(resampler_info.dst_buffer);

    if (state_buffer)
        free(state_buffer);

    if (controller[0]) SDL_GameControllerClose(controller[0]);
    if (controller[1]) SDL_GameControllerClose(controller[1]);

    SDL_Quit();
    return 0;
}
