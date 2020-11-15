#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "emu/nes_system.h"

#define TEXTURE_WIDTH   256
#define TEXTURE_HEIGHT  224

#define CONTROLLER_DEADZONE 2048

SDL_Rect            video_srcrect = {0,0,TEXTURE_WIDTH, TEXTURE_HEIGHT};
uint32_t*           texture_buffer = 0;
uint32_t            wnd_scale = 3;
SDL_Window*         wnd = 0;
SDL_Renderer*       renderer = 0;
SDL_Texture*        texture = 0;
SDL_GameController* controller[2];

nes_controller_state on_nes_input(int controller_id, void* client)
{
    nes_controller_state state;
    if (controller_id == 0)
    {
        const uint8_t* keys = SDL_GetKeyboardState(0);
        state.up    = keys[SDL_SCANCODE_W];
        state.down  = keys[SDL_SCANCODE_S];
        state.left  = keys[SDL_SCANCODE_A];
        state.right = keys[SDL_SCANCODE_D];
        state.A     = keys[SDL_SCANCODE_J];
        state.B     = keys[SDL_SCANCODE_K];
        state.select = keys[SDL_SCANCODE_TAB];
        state.start  = keys[SDL_SCANCODE_RETURN];

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
        return state;
    }
    else
    {
        memset(&state, 0, sizeof(nes_controller_state));
        return state;
    }
}

void on_nes_video(nes_video_output* video, void* client)
{
    const uint32_t colors[] = {
    0x545454, 0x741E00, 0x901008, 0x880030, 0x640044, 0x30005C, 0x000454, 0x00183C, 0x002A20, 0x003A08, 0x004000, 0x003C00, 0x3C3200, 0x000000, 0, 0,
    0x989698, 0xC44C08, 0xEC3230, 0xE41E5C, 0xB01488, 0x6414A0, 0x202298, 0x003C78, 0x005A54, 0x007228, 0x007C08, 0x287600, 0x786600, 0x000000, 0, 0,
    0xECEEEC, 0xEC9A4C, 0xEC7C78, 0xEC62B0, 0xEC54E4, 0xB458EC, 0x646AEC, 0x2088D4, 0x00AAA0, 0x00C474, 0x20D04C, 0x6CCC38, 0xCCB438, 0x3C3C3C, 0, 0, 
    0xECEEEC, 0xECCCA8, 0xECBCBC, 0xECB2D4, 0xECAEEC, 0xD4AEEC, 0xB0B4EC, 0x90C4E4, 0x78D2CC, 0x78DEB4, 0x90E2A8, 0xB4E298, 0xE4D6A0, 0xA0A2A0, 0, 0};
 
    nes_pixel* pixel = video->framebuffer;
    for (int y = 0; y < video->height; ++y)
    {
        for (int x = 0; x < video->width; ++x)
        {
            texture_buffer[y * 256 + x] = colors[(pixel + x)->value] | 0xFF000000;
        }
        pixel += NES_FRAMEBUFFER_ROW_STRIDE;
    }

    SDL_UpdateTexture(texture, 0, texture_buffer, sizeof(uint32_t) * TEXTURE_WIDTH);
    video_srcrect.w = video->width;
    video_srcrect.h = video->height;
}

void handle_shortcut_key(SDL_Scancode key)
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
    const char*     rom_path = argc > 1 ? argv[1] : "rom.nes";
    char            title[256];
    int             quit = 0;
    nes_config      config;
    nes_system*     system = 0;

    snprintf(title, 256, "NESM - %s", rom_path);

    config.client_data = 0;
    config.input_callback = &on_nes_input;
    config.video_callback = &on_nes_video;

    system = nes_system_create(rom_path, &config);
    if (!system)
    {
        fprintf(stderr, "Failed to initialized nes system.\n");
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

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, TEXTURE_WIDTH, TEXTURE_HEIGHT);
    if (!texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Failed to create texture: %s\n", SDL_GetError());
        return -1;
    }

    texture_buffer = (uint32_t*)malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(uint32_t));

    memset(&controller, 0, 2 * sizeof(SDL_GameController*));

    SDL_ShowWindow(wnd);

    while(!quit)
    {
        int w, h, has_key_up = 0;
        SDL_Scancode key_up_scancode = 0;
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
            handle_shortcut_key(key_up_scancode);

        SDL_GetWindowSize(wnd, &w, &h);
        if (((float)w / (float)h) >= aspect_ratio)
        {
            dstrect.y = 0;
            dstrect.h = h;
            dstrect.w = (float)h * aspect_ratio;
            dstrect.x = (w - dstrect.w)>>1;
        }
        else
        {
            dstrect.x = 0;
            dstrect.w = w;
            dstrect.h = (float)w / aspect_ratio;
            dstrect.y = (h - dstrect.h)>>1;
        }

        nes_system_frame(system);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &video_srcrect, &dstrect);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyWindow(wnd);

    nes_system_destroy(system);
    free(texture_buffer);

    if (controller[0]) SDL_GameControllerClose(controller[0]);
    if (controller[1]) SDL_GameControllerClose(controller[1]);

    SDL_Quit();
    return 0;
}
