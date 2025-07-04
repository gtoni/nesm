#include <memory.h>
#include <stdlib.h>
#include "nes_system.h"
#include "nes_rom.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include "emu6502.h"

// TODO:
// - 2nd controller handling

typedef struct nes_system_state
{
    cpu_state   cpu;
    nes_ppu     ppu;
    nes_apu     apu;
    int         cpu_odd_cycle;
    uint16_t    cpu_next_address;
    int         oam_dma;
    int         oam_dma_cycle;
    uint16_t    oam_dma_src_address;
    uint8_t     oam_dma_dst_address;
    int         dmc_dma;
    uint8_t     dmc_dma_dummy;
    uint16_t    dmc_dma_src_address;
    uint8_t     controller_input0;
    uint8_t     controller_input1;
    uint8_t     controller_read_timer0;
    uint8_t     ram[0x800];
    uint8_t     vram[0x1000];
    uint8_t     cached_ppu_reg[7];
    uint8_t     cached_apuio_reg[0x1F];
} nes_system_state;

struct nes_system
{
    nes_system_state    state;
    nes_config          config;
    nes_cartridge*      cartridge;
    uint16_t            framebuffer[SCANLINE_WIDTH * TOTAL_SCANLINES];
};

static void execute_memory_callbacks(nes_system* system, nes_memory_type memory_type, nes_memory_op op, uint16_t address, uint8_t* data)
{
    for (nes_system_layer* layer = system->config.layer; layer; layer = layer->next)
    {
        if (layer->memory_callback)
            layer->memory_callback(memory_type, op, address, data, layer->client_data);
    }
}

static void execute_cpu_callbacks(nes_system* system, cpu_state* state)
{
    for (nes_system_layer* layer = system->config.layer; layer; layer = layer->next)
    {
        if (layer->cpu_cycle_callback)
            layer->cpu_cycle_callback(state, layer->client_data);

        if (layer->cpu_callback && ((uint8_t)state->cycle) == 0)
            layer->cpu_callback(state, layer->client_data);
    }
}

static void execute_ppu_callbacks(nes_system* system, nes_ppu* ppu)
{
    for (nes_system_layer* layer = system->config.layer; layer; layer = layer->next)
    {
        if (layer->ppu_callback)
            layer->ppu_callback(ppu, layer->client_data);
    }
}

static void execute_apu_callbacks(nes_system* system, nes_apu* apu)
{
    for (nes_system_layer* layer = system->config.layer; layer; layer = layer->next)
    {
        if (layer->apu_callback)
            layer->apu_callback(apu, layer->client_data);
    }
}

/////////////////////////////////////////////////
// Internal
/////////////////////////////////////////////////

static void oam_dma_init(nes_system* system, uint8_t src_address, uint8_t dst_address)
{
    nes_system_state* state = &system->state;

    state->oam_dma = 1;
    state->oam_dma_cycle = 0;
    state->oam_dma_src_address = src_address << 8;
    state->oam_dma_dst_address = dst_address;
}

static int oam_dma_execute(nes_system* system)
{
    nes_system_state* state = &system->state;

    int cycle = state->oam_dma_cycle;
    int put_cycle = (cycle & 1);

    if (state->cpu_odd_cycle != put_cycle)
        return 1; // realign cycle

    if (put_cycle)
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_OAM, NES_MEMORY_OP_WRITE, state->oam_dma_dst_address & 0xFF, &state->cpu.data);

        state->ppu.primary_oam.bytes[(state->oam_dma_dst_address++) & 0xFF] = state->cpu.data;
    }
    else
    {
        state->cpu.address = state->oam_dma_src_address++;
    }

    if (++state->oam_dma_cycle == 512)
        state->oam_dma = 0;

    return !put_cycle;
}

static void dmc_dma_init(nes_system* system)
{
    nes_system_state* state = &system->state;

    state->dmc_dma = 1;
    state->dmc_dma_dummy = 1 + state->cpu.halted; // If already halted, perform a dummy halt cycle anyway
    state->dmc_dma_src_address = state->apu.dmc.current_address;
}

static int dmc_dma_execute(nes_system* system)
{
    nes_system_state* state = &system->state;

    int is_dmc_dma_cycle = 1;

    if (state->dmc_dma_dummy)
    {
        --state->dmc_dma_dummy;
        is_dmc_dma_cycle = 0;
    }

    if (state->cpu_odd_cycle == 1)
        is_dmc_dma_cycle = 0; // realign cycle

    if (!is_dmc_dma_cycle)
    {
        if (state->oam_dma)
            return oam_dma_execute(system);

        return 1;
    }

    if (state->dmc_dma_src_address >= 0x6000)
    {
        system->cartridge->mapper->read(system->cartridge, state->dmc_dma_src_address, &state->apu.dmc.sample_buffer);
    }
    else
    {
        state->apu.dmc.sample_buffer = state->ram[state->dmc_dma_src_address & 0x7FF];
    }

    execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_READ_DMA, state->dmc_dma_src_address, &state->apu.dmc.sample_buffer);

    state->apu.dmc.sample_buffer_loaded = 1;
    state->apu.dmc.bytes_remaining--;
    state->apu.dmc.current_address = 0x8000 + ((state->apu.dmc.current_address + 1) & 0x7FFF);

    if (!state->apu.dmc.bytes_remaining)
    {
        if (state->apu.dmc.loop)
        {
            state->apu.dmc.current_address = state->apu.dmc.sample_address;
            state->apu.dmc.bytes_remaining = state->apu.dmc.sample_length;
        }
        else
        {
            state->apu.dmc.interrupt = state->apu.dmc.irq_enabled;
        }
    }

    state->dmc_dma = 0;

    // Emulate bus conflict behavior
    uint16_t conflict_address = (state->cpu.address & 0xFFE0) | (state->dmc_dma_src_address & 0x1F);
    if (conflict_address == 0x4015 || conflict_address == 0x4016 || conflict_address == 0x4017)
    {
        state->cpu.address = conflict_address;
        return 1;
    }

    return 0;
}

static void ppu_mem_rw(nes_system* system)
{
    nes_system_state* state = &system->state;
    nes_mapper* mapper = system->cartridge->mapper;

    uint16_t address = state->ppu.vram_address & 0x3FFF;

    if (state->ppu.r)
    {
        mapper->ppu_read(system->cartridge, state->vram, address, &state->ppu.vram_data);

        execute_memory_callbacks(system, NES_MEMORY_TYPE_PPU, NES_MEMORY_OP_READ, state->ppu.vram_address, &state->ppu.vram_data);
    }
    else
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_PPU, NES_MEMORY_OP_WRITE, state->ppu.vram_address, &state->ppu.vram_data);

        mapper->ppu_write(system->cartridge, state->vram, address, state->ppu.vram_data);
    }
}

static void ppu_cpu_bus(nes_system* system)
{
    nes_system_state* state = &system->state;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ && state->cpu.address >= 0x2000 && state->cpu.address <= 0x3FFF)
    {
        uint8_t reg_addr = state->cpu.address & 7;

        state->cpu.data = state->ppu.reg_data;
        state->cached_ppu_reg[reg_addr] = state->ppu.reg_data;

        nes_memory_op read_op = (state->oam_dma || state->dmc_dma) ? NES_MEMORY_OP_READ_DMA : NES_MEMORY_OP_READ;
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, read_op, state->cpu.address, &state->cpu.data);

        if (reg_addr == NES_PPU_OAM_DATA_REG_ID)
            execute_memory_callbacks(system, NES_MEMORY_TYPE_OAM, read_op, state->ppu.oam_address, &state->ppu.reg_data);
    }
}

static void ppu_tick(nes_system* system)
{
    nes_system_state* state = &system->state;

    int is_reg_read = (state->ppu.reg_rw_mode == NES_PPU_REG_RW_MODE_READ);

    if (state->ppu.r | state->ppu.w)
        ppu_mem_rw(system);

    execute_ppu_callbacks(system, &state->ppu);

    nes_ppu_execute(&state->ppu);

    if (is_reg_read)
        ppu_cpu_bus(system);
    
    system->framebuffer[state->ppu.scanline * SCANLINE_WIDTH + state->ppu.dot] = state->ppu.color_out;

    if (system->config.video_callback && state->ppu.scanline == (RENDER_END_SCANLINE + 1) && state->ppu.dot == 0)
    {
        nes_video_output video_output;
        video_output.framebuffer = (nes_pixel*)(system->framebuffer + 2 + (NES_FRAMEBUFFER_ROW_STRIDE * 8));
        video_output.width  = 256;
        video_output.height = 224;
        video_output.odd_frame = !state->ppu.is_even_frame;

        system->config.video_callback(&video_output, system->config.client_data);
    }
}

static void cpu_mem_rw(nes_system* system)
{
    nes_system_state* state = &system->state;

    int is_ram      = state->cpu.address <  0x2000;
    int is_mapper   = state->cpu.address >= 0x6000;

    if (!(is_ram || is_mapper))
        return;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ)
    {
        if (is_ram)
            state->cpu.data = state->ram[state->cpu.address & 0x7FF];
        else
            system->cartridge->mapper->read(system->cartridge, state->cpu.address, &state->cpu.data);

        nes_memory_op read_op = (state->oam_dma || state->dmc_dma) ? NES_MEMORY_OP_READ_DMA : NES_MEMORY_OP_READ;
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, read_op, state->cpu.address, &state->cpu.data);
    }
    else if (state->cpu.rw_mode == CPU_RW_MODE_WRITE)
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_WRITE, state->cpu.address, &state->cpu.data);

        if (is_ram)
            state->ram[state->cpu.address & 0x7FF] = state->cpu.data;
        else
            system->cartridge->mapper->write(system->cartridge, state->cpu.address, state->cpu.data);
    }
}

static void cpu_ppu_bus(nes_system* system)
{
    nes_system_state* state = &system->state;
    int is_ppu_reg = state->cpu.address >= 0x2000 && state->cpu.address <= 0x3FFF;

    if (!is_ppu_reg)
        return;

    uint8_t reg_addr = state->cpu.address & 7;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ)
    {
        state->ppu.reg_rw_mode = NES_PPU_REG_RW_MODE_READ;
        state->ppu.reg_addr = reg_addr;
    }
    else if (state->cpu.rw_mode == CPU_RW_MODE_WRITE)
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_WRITE, state->cpu.address, &state->cpu.data);

        if (reg_addr == NES_PPU_OAM_DATA_REG_ID)
            execute_memory_callbacks(system, NES_MEMORY_TYPE_OAM, NES_MEMORY_OP_WRITE, state->ppu.oam_address, &state->cpu.data);

        state->ppu.reg_rw_mode = NES_PPU_REG_RW_MODE_WRITE;
        state->ppu.reg_addr = reg_addr;
        state->ppu.reg_data = state->cpu.data;
        state->cached_ppu_reg[reg_addr] = state->cpu.data;
    }
}

static void cpu_apu_bus(nes_system* system)
{
    nes_system_state* state = &system->state;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ && state->cpu.address == 0x4015)
    {
        state->apu.reg_rw_mode = NES_APU_REG_RW_MODE_READ;
        state->apu.reg_addr = 0x4015;
    }
    else if (state->cpu.rw_mode == CPU_RW_MODE_WRITE)
    {
        int is_apu_reg = state->cpu.address >= 0x4000 && state->cpu.address <= 0x4017 && 
                        state->cpu.address != 0x4009 && state->cpu.address != 0x400D && state->cpu.address != 0x4014 && state->cpu.address != 0x4016;

        if (!is_apu_reg)
            return;

        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_WRITE, state->cpu.address, &state->cpu.data);

        state->apu.reg_rw_mode = NES_APU_REG_RW_MODE_WRITE;
        state->apu.reg_addr = state->cpu.address;
        state->apu.reg_data = state->cpu.data;
        state->cached_apuio_reg[state->cpu.address & 0x1F] = state->cpu.data;
    }
}

static void cpu_joy_bus(nes_system* system)
{
    nes_system_state* state = &system->state;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ)
    {
        if (state->cpu.address == 0x4016)
        {
            uint8_t input = (state->controller_input0 >> 7) & 1;

            state->cpu.data = (state->cpu.data & 0xE0) | (input & 0x1F);

            if (state->controller_read_timer0 == 0)
                state->controller_input0 = (state->controller_input0 << 1) | 1;

            state->controller_read_timer0 = 2;

            state->cached_apuio_reg[state->cpu.address & 0x1F] = state->cpu.data;

            nes_memory_op read_op = (state->oam_dma || state->dmc_dma) ? NES_MEMORY_OP_READ_DMA : NES_MEMORY_OP_READ;
            execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, read_op, state->cpu.address, &state->cpu.data);
        }
    }
    else if (state->cpu.rw_mode == CPU_RW_MODE_WRITE && state->cpu.address == 0x4016)
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_WRITE, state->cpu.address, &state->cpu.data);

        if (state->cpu.data == 1)
        {
            state->controller_input0 = 0;
            state->controller_input1 = 0;
        }
        else
        {
            if (system->config.input_callback)
            {
                nes_controller_state controller_state = system->config.input_callback(0, system->config.client_data);
                memcpy(&state->controller_input0, &controller_state, 1);
            }
        }

        state->cached_apuio_reg[state->cpu.address & 0x1F] = state->cpu.data;
    }
}

static void cpu_oam_dma_bus(nes_system* system)
{
    nes_system_state* state = &system->state;

    if (state->cpu.rw_mode == CPU_RW_MODE_WRITE && state->cpu.address == 0x4014)
    {
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, NES_MEMORY_OP_WRITE, state->cpu.address, &state->cpu.data);

        state->cached_apuio_reg[state->cpu.address & 0x1F] = state->cpu.data;
        oam_dma_init(system, state->cpu.data, state->ppu.oam_address);
    }
}

static void cpu_tick(nes_system* system)
{
    nes_system_state* state = &system->state;

    state->cpu.address = state->cpu_next_address; // DMA may have hijacked the address, restore it
    if (!state->cpu.halted)
        execute_cpu_callbacks(system, &state->cpu);

    state->cpu = cpu_execute(state->cpu);
    state->cpu_next_address = state->cpu.address;

    state->cpu.rdy = !(state->dmc_dma || state->oam_dma);

    if (state->cpu.rdy || !state->cpu.halted || (state->dmc_dma ? dmc_dma_execute(system) : oam_dma_execute(system)))
    {
        cpu_mem_rw(system);
        cpu_ppu_bus(system);
        cpu_apu_bus(system);
        cpu_joy_bus(system);
        cpu_oam_dma_bus(system);
    }

    state->controller_read_timer0 >>= 1;

    state->cpu_odd_cycle ^= 1; 
}

static void apu_cpu_bus(nes_system* system)
{
    nes_system_state* state = &system->state;

    if (state->cpu.rw_mode == CPU_RW_MODE_READ && state->cpu.address == 0x4015)
    {
        state->cpu.data = state->apu.reg_data;
        state->cached_apuio_reg[0x15] = state->cpu.data;

        nes_memory_op read_op = (state->oam_dma || state->dmc_dma) ? NES_MEMORY_OP_READ_DMA : NES_MEMORY_OP_READ;
        execute_memory_callbacks(system, NES_MEMORY_TYPE_CPU, read_op, state->cpu.address, &state->cpu.data);
    }
}

static void apu_tick(nes_system* system)
{
    nes_system_state* state = &system->state;

    execute_apu_callbacks(system, &state->apu);

    nes_apu_execute(&state->apu);

    apu_cpu_bus(system);

    if (state->dmc_dma == 0 && state->apu.dmc.sample_buffer_loaded == 0 && state->apu.dmc.bytes_remaining)
        dmc_dma_init(system);

    if (state->dmc_dma && state->apu.dmc.bytes_remaining == 0)
        state->dmc_dma = 0;

    if (system->config.audio_callback && state->apu.sample_count == NES_APU_MAX_SAMPLES)
    {
        nes_audio_output audio;
        audio.samples = state->apu.samples;
        audio.sample_count = state->apu.sample_count;
        audio.sample_rate = 1789773;

        system->config.audio_callback(&audio, system->config.client_data);
    }
}

/////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////

nes_system* nes_system_create(nes_config* config)
{
    nes_cartridge* cartridge = 0;

    if (config->source_type == NES_SOURCE_FILE)
    {
        cartridge = nes_rom_load_cartridge(config->source.file_path);
    }
    else if (config->source_type == NES_SOURCE_MEMORY)
    {
        cartridge = nes_rom_create_cartridge(config->source.memory.data, config->source.memory.data_size);
    }
    else if (config->source_type == NES_SOURCE_CARTRIGE)
    {
        cartridge = config->source.cartridge;
    }

    if (!cartridge)
        return 0;

    nes_system* system  = (nes_system*)malloc(sizeof(nes_system));
    system->cartridge   = cartridge;
    system->config      = *config;

    nes_system_reset(system, NES_SYSTEM_RESET_POWER_UP);

    return system;
}

void nes_system_destroy(nes_system* system)
{
    if (system->config.source_type != NES_SOURCE_CARTRIGE)
        free(system->cartridge);

    free(system);
}

void nes_system_reset(nes_system* system, nes_system_reset_type reset_type)
{
    nes_system_state* state = &system->state;

    if (reset_type == NES_SYSTEM_RESET_POWER_UP)
    {
        state->cpu = cpu_power_up();

        for (uint32_t i = 0; i < 0x800; ++i)
            system->state.ram[i] = (uint8_t)rand();

        nes_apu_power_up(&system->state.apu);
    }
    else
    {
        state->cpu = cpu_reset(state->cpu);
        nes_apu_reset(&state->apu);
    }

    nes_ppu_reset(&state->ppu);
    state->cpu_odd_cycle = 1;
    state->dmc_dma = 0;
    state->oam_dma = 0;
    state->oam_dma_cycle = 0;
    state->oam_dma_src_address = 0;
    state->oam_dma_dst_address = 0;
    state->controller_input0 = 0;
    state->controller_input1 = 1;
}

size_t nes_system_get_state_size(nes_system *system)
{
    return sizeof(nes_system_state) + system->cartridge->state_size;
}

int nes_system_save_state(nes_system* system, void* buffer, size_t buffer_size)
{
    size_t state_size = nes_system_get_state_size(system);
    
    if (buffer && buffer_size >= state_size)
    {
        memcpy(buffer,                                   &system->state,            sizeof(nes_system_state));
        memcpy((char*)buffer + sizeof(nes_system_state), system->cartridge->state,  system->cartridge->state_size);
        return 1;
    }

    return 0;
}

int nes_system_load_state(nes_system* system, const void* buffer, size_t buffer_size)
{
    size_t state_size = nes_system_get_state_size(system);
    
    if (buffer && buffer_size >= state_size)
    {
        memcpy(&system->state,           buffer,                                    sizeof(nes_system_state));
        memcpy(system->cartridge->state, (char*)buffer + sizeof(nes_system_state),  system->cartridge->state_size);
        return 1;
    }

    return 0;
}

static uint8_t nes_system_read_cpu_byte(nes_system* system, uint16_t address)
{ 
    if (address >= 0x6000)
    {
        uint8_t data;
        system->cartridge->mapper->read(system->cartridge, address, &data);
        return data;
    }
    else
    {
        if (address < 0x2000)
        {
            return system->state.ram[address & 0x7FF];
        }
        else
        {
            if (address < 0x4000)
            {
                return system->state.cached_ppu_reg[address & 7];
            }
            else if (address < 0x4020)
            {
                return system->state.cached_apuio_reg[address & 0x1F];
            }
        }
    }

    return 0; 
}

static uint8_t nes_system_read_ppu_byte(nes_system* system, uint16_t address)
{ 
    if (address < 0x3F00)
    {
        uint8_t data = 0;
        system->cartridge->mapper->ppu_read(system->cartridge, system->state.vram, address, &data);
        return data;
    }
    else if (address <= 0x3FFF)
    {
        uint8_t palette_index = address & 0x1F;
        if ((palette_index & 0x13) == 0x10)
            palette_index &= ~0x10;

        return system->state.ppu.palettes[palette_index];
    }

    return 0; 
}

static uint8_t nes_system_read_oam_byte(nes_system* system, uint16_t address)
{
    return system->state.ppu.primary_oam.bytes[address & 0xFF];
}

void nes_system_read_memory(nes_system* system, nes_memory_type memory_type, uint16_t address, void* buffer, size_t buffer_size)
{
    uint8_t (*read_byte)(nes_system*, uint16_t) = 0;
    switch (memory_type)
    {
        case NES_MEMORY_TYPE_CPU : read_byte = nes_system_read_cpu_byte; break;
        case NES_MEMORY_TYPE_PPU : read_byte = nes_system_read_ppu_byte; break;
        case NES_MEMORY_TYPE_OAM : read_byte = nes_system_read_oam_byte; break;
        default: break;
    }

    if (!read_byte)
        return;

    for (uint16_t i = 0; i < buffer_size; ++i)
        *((uint8_t*)buffer + i) = read_byte(system, address + i);
}

void nes_system_tick(nes_system* system)
{
    nes_system_state* state = &system->state;

    int had_vbl = state->ppu.vbl;

    ppu_tick(system);
    ppu_tick(system);
    ppu_tick(system);

    if (!had_vbl && state->ppu.vbl)
        state->cpu.nmi = 1;
 
    state->cpu.irq = state->apu.frame_interrupt | state->apu.dmc.interrupt;
    system->cartridge->mapper->tick(system->cartridge, &state->cpu, &state->ppu);

    apu_tick(system);
    cpu_tick(system);
}

void nes_system_frame(nes_system* system)
{
    for (int i = 0; i < 29781; ++i)
        nes_system_tick(system);
}
