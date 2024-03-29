#include <memory.h>
#include "nes_system.h"
#include "nes_rom.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include "emu6502.h"

// todo:
// - PPU leftmost clipping
// - proper CHR RAM
// - 2nd controller handling
// - APU

struct nes_system
{
    nes_config      config;
    nes_cartridge*  cartridge;
    nes_mapper*     mapper;
    cpu_state       cpu;
    nes_ppu         ppu;
    nes_apu         apu;
    int             own_cartridge;
    int             cpu_odd_cycle;
    int             oam_dma;
    uint8_t         oam_dma_data;
    int             oam_dma_cycle;
    uint16_t        oam_dma_src_address;
    uint8_t         oam_dma_dst_address;
    int             dmc_dma;
    uint8_t         dmc_dma_stall;
    uint16_t        dmc_dma_src_address;
    uint8_t         controller_input0;
    uint8_t         controller_input1;
    uint8_t         framebuffer[SCANLINE_WIDTH * TOTAL_SCANLINES];
    uint8_t         vram[0x2000];
    uint8_t         ram[0x800];
};

nes_system* nes_system_create(const char* rom_path, nes_config* config)
{
    nes_system* system = (nes_system*)malloc(sizeof(nes_system));
    system->cartridge = nes_rom_load_cartridge(rom_path);
    system->own_cartridge = 1;
    system->config = *config;
    system->mapper = system->cartridge->mapper;
    nes_system_reset(system);
    return system;
}

void nes_system_destroy(nes_system* system)
{
    if (system->own_cartridge)
    {
        free(system->cartridge);
    }
    free(system);
}

void nes_system_reset(nes_system* system)
{
    nes_ppu_reset(&system->ppu);
    nes_apu_reset(&system->apu);
    system->cpu = cpu_reset();
    system->cpu_odd_cycle = 0;
    system->oam_dma = 0;
    system->oam_dma_data = 0;
    system->oam_dma_cycle = 0;
    system->oam_dma_src_address = 0;
    system->oam_dma_dst_address = 0;
    system->controller_input0 = 0;
    system->controller_input1 = 1;
}

//////////////

void ppu_rw_bus(nes_system* system)
{
    uint16_t address = system->ppu.vram_address & 0x3FFF;

    if (address < 0x2000)
    {
        if (system->cartridge->chr_rom_size > address)
        {
            if (system->ppu.r)
                system->ppu.vram_data = system->mapper->read_chr(system->cartridge, address);
        }
        else
        {
            // CHR_RAM hack
            static uint8_t chr_ram[0x2000];
            if (system->ppu.r)
                system->ppu.vram_data = chr_ram[address];
            else
                chr_ram[address] = system->ppu.vram_data;
        }
    }
    else
    {
        address -= 0x2000;
        if (address >= 0x1F00 && address <= 0x1FFF)
        {
            address = 0xF00 | (address & 0xFF);
        }

        if (address <= 0xFFF)
        {
            switch (system->cartridge->mirroring)
            {
                case NES_NAMETABLE_MIRRORING_VERTICAL:
                    address = address & 0x7FF;
                    break;
                case NES_NAMETABLE_MIRRORING_HORIZONTAL:
                    address = ((address/2)&0x400) + (address & 0x3FF);
                    break;
                case NES_NAMETABLE_MIRRORING_SINGLE_LOW:
                    address = address & 0x3FF;
                    break;
                case NES_NAMETABLE_MIRRORING_SINGLE_HIGH:
                    address = 0x800 + (address & 0x3FF);
                    break;
            }
        }

        if (system->ppu.r)
            system->ppu.vram_data = system->vram[address];
        else
            system->vram[address] = system->ppu.vram_data;
    }
}

void ppu_tick(nes_system* system)
{
    if (system->ppu.r | system->ppu.w)
        ppu_rw_bus(system);

    nes_ppu_execute(&system->ppu);
    
    system->framebuffer[system->ppu.scanline * SCANLINE_WIDTH + system->ppu.dot] = system->ppu.color_out;
   
    if (system->ppu.scanline == (LAST_RENDER_SCANLINE + 1) && system->ppu.dot == 0)
    {
        nes_video_output video_output;
        video_output.framebuffer = (nes_pixel*)(system->framebuffer + 2 + (NES_FRAMEBUFFER_ROW_STRIDE * 8));
        video_output.width = 256;
        video_output.height = 224;
        video_output.odd_frame = !system->ppu.is_even_frame;
        video_output.emphasize_red = system->ppu.render_mask & NES_PPU_RENDER_MASK_EMPHASIZE_RED;
        video_output.emphasize_green = system->ppu.render_mask & NES_PPU_RENDER_MASK_EMPHASIZE_GREEN;
        video_output.emphasize_blue = system->ppu.render_mask & NES_PPU_RENDER_MASK_EMPHASIZE_BLUE;

        system->config.video_callback(&video_output, system->config.client_data);
    }
}

void oam_dma_init(nes_system* system, uint8_t src_address, uint8_t dst_address)
{
    system->oam_dma = 1;
    system->oam_dma_cycle = system->cpu_odd_cycle;
    system->oam_dma_src_address = src_address << 8;
    system->oam_dma_dst_address = dst_address;
}

void oam_dma_execute(nes_system* system)
{
    int cur = system->oam_dma_cycle++;
    if (cur == 1)
    {
        // dummy cycle
    }
    else if (cur > 1 && cur < 514)
    {
        if (cur % 2)
        {
            system->ppu.primary_oam.bytes[(system->oam_dma_dst_address++)&0xFF] = system->oam_dma_data;
        }
        else
        {
            if (system->oam_dma_src_address >= 0x6000)
            {
                system->oam_dma_data = system->mapper->read(system->cartridge, system->oam_dma_src_address);
            }
            else
            {
                system->oam_dma_data = system->ram[system->oam_dma_src_address & 0x7FF];
            }
            system->oam_dma_src_address++;
        }
    }
    else if (cur == 514)
    {
        system->oam_dma = 0;
    }
}

void dmc_dma_init(nes_system* system)
{
    system->dmc_dma = 1;
    system->dmc_dma_src_address = system->apu.dmc.current_address;

    if (system->oam_dma)
    {
        if (system->oam_dma_cycle == 514)       system->dmc_dma_stall = 3;
        else if (system->oam_dma_cycle == 513)  system->dmc_dma_stall = 1;
        else                                    system->dmc_dma_stall = 2;
    }
    else
    {
        system->dmc_dma_stall = 4;
        if (system->cpu.rw_mode == CPU_RW_MODE_WRITE)
        {
            cpu_state cpuNext = cpu_execute(system->cpu);
            if (cpuNext.rw_mode != CPU_RW_MODE_WRITE)
                system->dmc_dma_stall = 3;
        }
    }
}

void dmc_dma_execute(nes_system* system)
{
    if (--system->dmc_dma_stall == 0)
    {
        if (system->dmc_dma_src_address >= 0x6000)
        {
            system->apu.dmc.sample_buffer = system->mapper->read(system->cartridge, system->dmc_dma_src_address);
        }
        else
        {
            system->apu.dmc.sample_buffer = system->ram[system->dmc_dma_src_address & 0x7FF];
        }

        system->apu.dmc.sample_buffer_loaded = 1;
        system->apu.dmc.bytes_remaining--;
        system->apu.dmc.current_address = 0x8000 + ((system->apu.dmc.current_address + 1) & 0x7FFF);

        if (!system->apu.dmc.bytes_remaining)
        {
            if (system->apu.dmc.loop)
            {
                system->apu.dmc.current_address = system->apu.dmc.sample_address;
                system->apu.dmc.bytes_remaining = system->apu.dmc.sample_length;
            }
            else
            {
                system->apu.dmc.interrupt = system->apu.dmc.irq_enabled;
            }
        }

        system->dmc_dma = 0;
    }
}

void cpu_rw_bus(nes_system* system)
{
    if (system->cpu.rw_mode == CPU_RW_MODE_READ)
    {
        if (system->cpu.address >= 0x6000)
        {
            system->cpu.data = system->mapper->read(system->cartridge, system->cpu.address);
        }
        else
        {
            if (system->cpu.address < 0x2000)
            {
                system->cpu.data = system->ram[system->cpu.address & 0x7FF];
            }
            else
            {
                if (system->cpu.address < 0x4000)
                {
                    system->ppu.reg_rw_mode = NES_PPU_REG_RW_MODE_READ;
                    system->ppu.reg_addr = system->cpu.address & 7;
                    system->cpu.data = system->ppu.reg_data;
                }
                else
                {
                    switch (system->cpu.address)
                    {
                        case 0x4016: // Controller 0
                        {
                            system->cpu.data = (system->controller_input0 >> 7) & 1;
                            system->controller_input0 = (system->controller_input0 << 1) | 1;
                        }
                        break;
                        case 0x4015: // APU Status
                        {
                            system->apu.reg_rw_mode = NES_APU_REG_RW_MODE_READ;
                            system->apu.reg_addr = 0x4015;
                        }
                        break;
                    }
                }
            }
        }
    }
    else if (system->cpu.rw_mode == CPU_RW_MODE_WRITE)
    {
        if (system->cpu.address >= 0x6000)
        {
            system->mapper->write(system->cartridge, system->cpu.address, system->cpu.data);
        }
        else
        {
            if (system->cpu.address < 0x2000)
            {
                system->ram[system->cpu.address & 0x7FF] = system->cpu.data;
            }
            else
            {
                if (system->cpu.address < 0x4000)
                {
                    system->ppu.reg_rw_mode = NES_PPU_REG_RW_MODE_WRITE;
                    system->ppu.reg_addr = system->cpu.address & 7;
                    system->ppu.reg_data = system->cpu.data;
                }
                else
                {
                    switch (system->cpu.address)
                    {
                        case 0x4014:
                        {
                            oam_dma_init(system, system->cpu.data, system->ppu.oam_address);
                        }
                        break;
                        case 0x4016:
                        {
                            if (system->cpu.data == 1)
                            {
                                system->controller_input0 = 0;
                                system->controller_input1 = 0;
                            }
                            else
                            {
                                nes_controller_state state = system->config.input_callback(0, system->config.client_data);
                                memcpy(&system->controller_input0, &state, 1);
                            }
                        }
                        break;
                        // APU
                        case 0x4000: case 0x4001: case 0x4002: case 0x4003: // Pulse 1
                        case 0x4004: case 0x4005: case 0x4006: case 0x4007: // Pulse 2
                        case 0x4008: case 0x400A: case 0x400B: // Triangle
                        case 0x400C: case 0x400E: case 0x400F: // Noise
                        case 0x4010: case 0x4011: case 0x4012: case 0x4013: // DMC
                        case 0x4015: // Status 
                        case 0x4017: // Frame counter
                        {
                            system->apu.reg_rw_mode = NES_APU_REG_RW_MODE_WRITE;
                            system->apu.reg_addr = system->cpu.address;
                            system->apu.reg_data = system->cpu.data;
                        }
                        break;
                    }
                }
            }
        }
    }
}

void apu_rw_bus(nes_system* system)
{
    if (system->cpu.rw_mode == CPU_RW_MODE_READ && system->cpu.address == 0x4015)
        system->cpu.data = system->apu.reg_data;

    if (system->dmc_dma == 0 && system->apu.dmc.sample_buffer_loaded == 0 && system->apu.dmc.bytes_remaining)
        dmc_dma_init(system);
}

void apu_tick(nes_system* system)
{
    nes_apu_execute(&system->apu);

    apu_rw_bus(system);

    if (system->apu.sample_count == 29781)
    {
        nes_audio_output audio;
        audio.samples = system->apu.samples;
        audio.sample_count = system->apu.sample_count;
        audio.sample_rate = 1789773;

        system->config.audio_callback(&audio, system->config.client_data);
        system->apu.sample_count = 0;
    }
}

void cpu_tick(nes_system* system)
{
    system->cpu = cpu_execute(system->cpu);

    cpu_rw_bus(system);
}

void nes_system_tick(nes_system* system)
{
    int had_vbl = system->ppu.vbl;

    ppu_tick(system);

    // Transfer data read from PPU to CPU immediately AFTER the PPU processes the read request
    if (system->cpu.rw_mode == CPU_RW_MODE_READ && system->cpu.address >= 0x2000 && system->cpu.address <= 0x3F00)
    {
        system->cpu.data = system->ppu.reg_data;
    }

    ppu_tick(system);
    ppu_tick(system);

    if (!had_vbl && system->ppu.vbl)
        system->cpu.nmi = 1;

    apu_tick(system);
    system->cpu.irq = system->apu.frame_interrupt | system->apu.dmc.interrupt;

    if (system->dmc_dma)
    {
        dmc_dma_execute(system);
    }
    else if (system->oam_dma)
    {
        oam_dma_execute(system);
    }
    else
    {
        cpu_tick(system);
    }
    
    system->cpu_odd_cycle ^= 1; 
}

void nes_system_frame(nes_system* system)
{
    for (int i = 0; i < 29781; ++i)
        nes_system_tick(system);
}


