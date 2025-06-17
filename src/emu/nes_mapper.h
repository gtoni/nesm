#ifndef _NES_MAPPER_H_
#define _NES_MAPPER_H_

#include <stdlib.h>
#include <stdint.h>

#include "nes_ppu.h"
#include "nes_cartridge.h"
#include "emu6502.h"

typedef struct nes_mapper
{
    size_t  state_size;
    void    (*init)(nes_cartridge*);
    void    (*read)(nes_cartridge*, uint16_t address, uint8_t* out_data);
    void    (*write)(nes_cartridge*, uint16_t address, uint8_t data);
    void    (*ppu_read)(nes_cartridge*, uint8_t* vram, uint16_t address, uint8_t* out_data);
    void    (*ppu_write)(nes_cartridge*, uint8_t* vram, uint16_t address, uint8_t data);
    void    (*tick)(nes_cartridge*, cpu_state*, nes_ppu*);
} nes_mapper;

// NROM

static void NROM_init(nes_cartridge* cartridge){}
static void NROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data){}

static void NROM_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    if (cartridge->prg_rom_size == 0x8000)  *out_data = *(cartridge->prg_rom + (address & 0x7FFF));
    else                                    *out_data = *(cartridge->prg_rom + (address & 0x3FFF));
}

static void NROM_tick(nes_cartridge* cartridge, cpu_state* cpu, nes_ppu* ppu) {}

static uint16_t NROM_nametable_address(nes_cartridge* cartridge, uint16_t address)
{
    address = ((address & 0x3FFF) - 0x2000) & 0x0FFF;
    switch (cartridge->mirroring)
    {
        case NES_NAMETABLE_MIRRORING_VERTICAL:      address = address & 0x7FF; break;
        case NES_NAMETABLE_MIRRORING_HORIZONTAL:    address = ((address / 2) & 0x400) + (address & 0x3FF); break;
        case NES_NAMETABLE_MIRRORING_SINGLE_LOW:    address = address & 0x3FF; break;
        case NES_NAMETABLE_MIRRORING_SINGLE_HIGH:   address = 0x800 + (address & 0x3FF); break;
    }
    return address;
}

static void NROM_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    if (address < 0x2000)
    {
        if (address < cartridge->chr_rom_size)      *out_data = cartridge->chr_rom[address];
        else if (address < cartridge->chr_ram_size) *out_data = cartridge->chr_ram[address];
    }
    else
    {
        address = NROM_nametable_address(cartridge, address);
        *out_data = vram[address];
    }
}

static void NROM_ppu_write(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t data)
{
    if (address < 0x2000)
    {
        if (address >= cartridge->chr_rom_size && address < cartridge->chr_ram_size)
            cartridge->chr_ram[address] = data;
    }
    else
    {
        address = NROM_nametable_address(cartridge, address);
        vram[address] = data;
    }
}

static nes_mapper nes_mapper_get_NROM()
{
    nes_mapper nrom = {0, &NROM_init, &NROM_read, &NROM_write, &NROM_ppu_read, &NROM_ppu_write, &NROM_tick };
    return nrom;
}

// AxROM

typedef struct axrom_mapper_state
{
    size_t   bank_offset;
    uint16_t mirroring;

} axrom_mapper_state;

static void AxROM_init(nes_cartridge* cartridge)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->state;
    size_t num_banks = cartridge->prg_rom_size / 0x8000;
    state->bank_offset = (num_banks - 1) * 0x8000; 
    state->mirroring = 0;
}

static void AxROM_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->state;
    *out_data = cartridge->prg_rom[(state->bank_offset + (address - 0x8000)) % cartridge->prg_rom_size];
}

static void AxROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->state;
    state->bank_offset = (data & 7) * 0x8000;
    state->mirroring = ((uint16_t)data & 0x10) << 7;
}

static uint16_t AxROM_nametable_address(nes_cartridge* cartridge, uint16_t address)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->state;
    return state->mirroring + (address & 0x3FF);
}

static void AxROM_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    if (address < 0x2000)
    {
        NROM_ppu_read(cartridge, vram, address, out_data);
    }
    else
    {
        address = AxROM_nametable_address(cartridge, address);
        *out_data = vram[address];
    }
}

static void AxROM_ppu_write(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t data)
{
    if (address < 0x2000)
    {
        NROM_ppu_write(cartridge, vram, address, data);
    }
    else
    {
        address = AxROM_nametable_address(cartridge, address);
        vram[address] = data;
    }
}

static nes_mapper nes_mapper_get_AxROM()
{
    nes_mapper unrom = {sizeof(axrom_mapper_state), &AxROM_init, &AxROM_read, &AxROM_write, &AxROM_ppu_read, &AxROM_ppu_write, &NROM_tick };
    return unrom;
}

// UxROM

typedef struct uxrom_mapper_state
{
    size_t  current_bank_offset;
    size_t  fixed_bank_offset;
    uint8_t bank_mask;
} uxrom_mapper_state;

static void UxROM_init(nes_cartridge* cartridge)
{
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->state;
    state->fixed_bank_offset = cartridge->prg_rom_size - 0x4000;
    state->current_bank_offset = 0;

    size_t num_banks = cartridge->prg_rom_size / 0x4000;

    if (num_banks < 8)          state->bank_mask = 0x03;
    else if (num_banks < 16)    state->bank_mask = 0x07;
    else                        state->bank_mask = 0x0F;
}

static void UxROM_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->state;
    if (address >= 0xC000)  *out_data = cartridge->prg_rom[state->fixed_bank_offset + (address - 0xC000)];
    else                    *out_data = cartridge->prg_rom[state->current_bank_offset + (address - 0x8000)];
}

static void UxROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->state;
    if (address >= 0x8000)
        state->current_bank_offset = (data & state->bank_mask) * 0x4000;
}

static nes_mapper nes_mapper_get_UxROM()
{
    nes_mapper unrom = {sizeof(uxrom_mapper_state), &UxROM_init, &UxROM_read, &UxROM_write, &NROM_ppu_read, &NROM_ppu_write, &NROM_tick };
    return unrom;
}

// Mapper071

typedef struct mapper071_mapper_state
{
    uxrom_mapper_state uxrom_state;
    uint16_t mirroring_mode;
    uint8_t  mirroring_set;
} mapper071_mapper_state;

static void Mapper071_init(nes_cartridge* cartridge)
{
    UxROM_init(cartridge);

    mapper071_mapper_state* state = (mapper071_mapper_state*)cartridge->state;
    state->mirroring_mode = 0;
    state->mirroring_set = 0;
}

static void Mapper071_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    mapper071_mapper_state* state = (mapper071_mapper_state*)cartridge->state;
    if (address >= 0x9000 && address <= 0x9FFF)
    {
        state->mirroring_mode = ((uint16_t)data << 7) & 0x800;
        state->mirroring_set = 1;
    }

    if (!state->mirroring_set || address >= 0xC000)
        UxROM_write(cartridge, address, data);
}

static uint16_t Mapper071_nametable_address(nes_cartridge* cartridge, uint16_t address)
{
    mapper071_mapper_state* state = (mapper071_mapper_state*)cartridge->state;
    if (state->mirroring_set)
    {
        return state->mirroring_mode + (address & 0x3FF);
    }
    else
    {
        return ((address & 0x3FFF) - 0x2000) & 0x7FF;
    }
}

static void Mapper071_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    if (address < 0x2000)
    {
        NROM_ppu_read(cartridge, vram, address, out_data);
    }
    else
    {
        address = Mapper071_nametable_address(cartridge, address);
        *out_data = vram[address];
    }
}

static void Mapper071_ppu_write(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t data)
{
    if (address < 0x2000)
    {
        NROM_ppu_write(cartridge, vram, address, data);
    }
    else
    {
        address = Mapper071_nametable_address(cartridge, address);
        vram[address] = data;
    }
}

static nes_mapper nes_mapper_get_Mapper071()
{
    nes_mapper unrom = {sizeof(mapper071_mapper_state), &Mapper071_init, &UxROM_read, &Mapper071_write, &Mapper071_ppu_read, &Mapper071_ppu_write, &NROM_tick };
    return unrom;
}

// CNROM

typedef struct cnrom_mapper_state
{
    size_t  current_bank_offset;
    uint8_t ram[0x800];
}cnrom_mapper_state;

static void CNROM_init(nes_cartridge* cartridge)
{
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->state;
    state->current_bank_offset = 0;
}

static void CNROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->state;
    if (address >= 0x6000 && address < 0x8000)
    {
        state->ram[address & 0x7FF] = data;
    }
    else if (address >= 0x8000)
    {
        state->current_bank_offset = (data & 3) * 0x2000;
    }
}

static void CNROM_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->state;
    if (address >= 0x6000 && address < 0x8000)
    {
        *out_data = state->ram[address & 0x7FF];
    }
    else
    {
        if (cartridge->prg_rom_size == 0x8000)  *out_data = *(cartridge->prg_rom + (address & 0x7FFF));
        else                                    *out_data = *(cartridge->prg_rom + (address & 0x3FFF));
    }
}

static void CNROM_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    if (address < 0x2000)
    {
        cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->state;
        *out_data = cartridge->chr_rom[state->current_bank_offset + address];
    }
    else
    {
        NROM_ppu_read(cartridge, vram, address, out_data);
    }
}

static nes_mapper nes_mapper_get_CNROM()
{
    nes_mapper unrom = {sizeof(cnrom_mapper_state), &CNROM_init, &CNROM_read, &CNROM_write, &CNROM_ppu_read, &NROM_ppu_write, &NROM_tick };
    return unrom;
}

// MMC1

typedef struct mmc1_mapper_state
{
    uint8_t  bank_mode      : 2;
    uint8_t  chr_bank_mode  : 1;
    uint8_t  write_enable   : 1;
    uint8_t  ram_enable     : 1;
    uint8_t  is_SUROM       : 1;
    size_t   prg_bank_selector;
    size_t   fixed_bank_offset;
    size_t   current_bank_offset;
    size_t   chr_bank_low_offset;
    size_t   chr_bank_high_offset;
    uint8_t  shift_reg;
    uint8_t  shift_count;
    uint8_t  ram[8192];
} mmc1_mapper_state;

static void MMC1_init(nes_cartridge* cartridge)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->state;
    state->bank_mode = 3;
    state->chr_bank_mode = 0;
    state->prg_bank_selector    = 0;
    state->fixed_bank_offset    = (cartridge->prg_rom_size - 0x4000) & 0x3FFFF;
    state->current_bank_offset  = 0;
    state->chr_bank_low_offset  = 0;
    state->chr_bank_high_offset = 0;
    state->shift_reg = 0;
    state->shift_count = 0;
    state->write_enable = 1;
    state->ram_enable = 1;
    state->is_SUROM = (cartridge->prg_rom_size == 512 * 1024);
}

static void MMC1_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->state;

    if (address >= 0x6000 && address <= 0x7FFF)
    {
        *out_data = state->ram[address - 0x6000];
    }
    else
    {
        size_t prg_address = state->prg_bank_selector;
        switch (state->bank_mode)
        {
            default:
            case 0: case 1:             prg_address |= state->current_bank_offset + (address - 0x8000); 
            break;
            case 2:
                if (address >= 0xC000)  prg_address |= state->current_bank_offset + (address - 0xC000);
                else                    prg_address |= state->fixed_bank_offset + (address - 0x8000);
            break;
            case 3:
                if (address >= 0xC000)  prg_address |= state->fixed_bank_offset + (address - 0xC000);
                else                    prg_address |= state->current_bank_offset + (address - 0x8000);
            break;
        }

        *out_data = cartridge->prg_rom[prg_address];
    }
}

static void MMC1_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->state;
    if (address >= 0x6000 && address <= 0x7FFF)
    {
        state->ram[address - 0x6000] = data;
    }
    else
    {
        if (data & 0x80) // Reset
        {
            state->shift_reg = 0;
            state->shift_count = 0;
            state->bank_mode = 3;
            state->fixed_bank_offset = (cartridge->prg_rom_size - 0x4000) & 0x3FFFF;
            state->write_enable = 1;
        }
        else if (state->write_enable)
        {
            state->write_enable = 0;

            data = ((data & 1) << 4) | (state->shift_reg >> 1);

            if (++state->shift_count < 5)
            {
                state->shift_reg = data;
            }
            else
            {
                switch ((address >> 13) & 3)
                {
                    case 0: // Control
                    {
                        cartridge->mirroring = (nes_nametable_mirroring)(data & 3);
                        state->bank_mode = (data >> 2) & 3;
                        state->chr_bank_mode = data >> 4;

                        switch (state->bank_mode)
                        {
                            default:
                            case 0: case 1:break;
                            case 2:
                                state->fixed_bank_offset = 0;
                            break;
                            case 3:
                                state->fixed_bank_offset = (cartridge->prg_rom_size - 0x4000) & 0x3FFFF;
                            break;
                        }
                    }
                    break;
                    case 1: // CHR bank 0
                    {
                        if (state->is_SUROM)
                        {
                            state->prg_bank_selector = (data >> 4) * 0x40000;
                            data &= 1;
                        }

                        unsigned m = 1 - state->chr_bank_mode;
                        unsigned bank = (data & 0x1F) & ~m;
                        state->chr_bank_low_offset = bank * (4 * 1024);
                    }
                    break;
                    case 2: // CHR bank 1
                    {
                        if (state->is_SUROM)
                        {
                            state->prg_bank_selector = (data >> 4) * 0x40000;
                            data &= 1;
                        }

                        state->chr_bank_high_offset = data * (4 * 1024);
                    }
                    break;
                    case 3: // PRG bank
                    {
                        unsigned m = state->bank_mode < 2;
                        unsigned bank = (data & 0x0F) & ~m;
                        state->current_bank_offset = (bank * 0x4000) % cartridge->prg_rom_size;
                        state->ram_enable = data & 0x10;
                    }
                    break;
                }

                state->shift_reg = 0;
                state->shift_count = 0; 
            }
        }
    }
}

static void MMC1_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    if (address < 0x2000)
    {
        mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->state;
        uint32_t chr_address;

        if (state->chr_bank_mode == 0 || address < 0x1000)
            chr_address = state->chr_bank_low_offset + address;
        else
            chr_address = state->chr_bank_high_offset + (address - 0x1000);

        if (chr_address < cartridge->chr_rom_size)      *out_data = cartridge->chr_rom[chr_address];
        else if (chr_address < cartridge->chr_ram_size) *out_data = cartridge->chr_ram[chr_address];
    }
    else
    {
        NROM_ppu_read(cartridge, vram, address, out_data);
    }
}


static void MMC1_tick(nes_cartridge* cartridge, cpu_state* cpu, nes_ppu* ppu)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->state;
    if ((cpu->cycle & 0xFF) == 0)
        state->write_enable = 1;
}

static nes_mapper nes_mapper_get_MMC1()
{
    nes_mapper unrom = {sizeof(mmc1_mapper_state), &MMC1_init, &MMC1_read, &MMC1_write, &MMC1_ppu_read, &NROM_ppu_write, &MMC1_tick };
    return unrom;
}

// MMC3

typedef struct mmc3_mapper_state
{
    int         nametable_4screen;
    int         nametable_arrangement;
    uint32_t    bank_select;
    int         bank_mode;
    int         chr_bank_mode;
    int         write_protect;
    int         ram_enable;
    int         irq_enable;
    int         has_irq;
    int         normal_irq_behaviour;
    uint8_t     irq_reload_value;
    int         irq_reload_flag;
    uint8_t     irq_counter;
    uint16_t    last_address;
    uint32_t    a12_edge_counter;
    uint32_t    banks[8];
    uint8_t     ram[0x2000];
}mmc3_mapper_state;

typedef struct mmc3_mapper_state_4screen
{
    mmc3_mapper_state   base;
    uint8_t             nametable_vram[0x1000];
}mmc3_mapper_state_4screen;

static void MMC3_init(nes_cartridge* cartridge)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;
    memset(state, 0, sizeof(mmc3_mapper_state));
    state->nametable_arrangement = 0;
    state->ram_enable = 1;
    state->normal_irq_behaviour = 1;
}

static void MMC3_init_4screen(nes_cartridge* cartridge)
{
    MMC3_init(cartridge);

    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;
    state->nametable_4screen = 1;
}

static void MMC3_read(nes_cartridge* cartridge, uint16_t address, uint8_t* out_data)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;
    if (address >= 0x6000 && address < 0x8000)
    {
        if (state->ram_enable)
            *out_data = state->ram[address - 0x6000];
    }
    else if (address >= 0x8000)
    {
        if (address >= 0xE000)
        {
            uint32_t bank_address = (cartridge->prg_rom_size - 0x2000);
            *out_data = cartridge->prg_rom[bank_address + (address - 0xE000)];
        }
        else
        {
            uint32_t prg_address = 0;
            if (state->bank_mode)
            {
                if (address >= 0xC000)      prg_address = state->banks[6] * 0x2000 + (address - 0xC000);
                else if (address >= 0xA000) prg_address = state->banks[7] * 0x2000 + (address - 0xA000);
                else                        prg_address = (cartridge->prg_rom_size - 0x4000) + (address - 0x8000);
            }
            else
            {
                if (address >= 0xC000)      prg_address = (cartridge->prg_rom_size - 0x4000) + (address - 0xC000);
                else if (address >= 0xA000) prg_address = state->banks[7] * 0x2000 + (address - 0xA000);
                else                        prg_address = state->banks[6] * 0x2000 + (address - 0x8000);
            }

            if (prg_address >= cartridge->prg_rom_size)
                prg_address = prg_address % cartridge->prg_rom_size;

            *out_data = cartridge->prg_rom[prg_address];
        }
    }
}

static void MMC3_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;
    if (address >= 0x6000 && address < 0x8000 && !state->write_protect)
        state->ram[address - 0x6000] = data;

    if (address >= 0x8000)
    {
        int odd = address & 1;

        if (address < 0xA000)
        {
            if (odd)
            {
                if (state->bank_select < 2)
                    data = data & 0xFE;
                else if (state->bank_select > 5)
                    data = data & 0x3F;

                state->banks[state->bank_select] = data;
            }
            else
            {
                state->bank_select   = data & 7;
                state->bank_mode     = (data >> 6) & 1;
                state->chr_bank_mode = (data >> 7) & 1;
            }
        }
        else if (address < 0xC000)
        {
            if (odd)
            {
                state->write_protect = (data >> 6) & 1;
                state->ram_enable    = (data >> 7) & 1;
            }
            else
            {
                state->nametable_arrangement = data & 1;
            }
        }
        else if (address < 0xE000)
        {
            if (odd)
            {
                state->irq_reload_flag = 1;
            }
            else
            {
                state->irq_reload_value = data;
            }
        }
        else
        {
            if (odd)
            {
                state->irq_enable = 1;
            }
            else
            {
                state->irq_enable = 0;
                state->has_irq = 0;
            }
        }
    }
}

static uint16_t MMC3_nametable_address(mmc3_mapper_state* state, uint16_t address)
{
    address = ((address & 0x3FFF) - 0x2000) & 0x0FFF;

    if (state->nametable_arrangement)
        return ((address / 2) & 0x400) + (address & 0x3FF);
    else
        return address & 0x7FF;
}

static int MMC3_check_A12(mmc3_mapper_state* state, uint16_t address)
{
    state->last_address = address;

    if (address & 0x1000)
    {
        if (state->a12_edge_counter == 0)
        {
            uint32_t prev = state->irq_counter;

            if (state->irq_counter == 0 || state->irq_reload_flag)
                state->irq_counter = state->irq_reload_value;
            else
                state->irq_counter--;
            
            if (state->irq_counter == 0 && (state->normal_irq_behaviour || prev || state->irq_reload_flag))
                state->has_irq = state->irq_enable;

            state->irq_reload_flag = 0;
        }

        state->a12_edge_counter = 3;
        return 1;
    }
    return 0;
}

static void MMC3_ppu_read(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t* out_data)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;

    MMC3_check_A12(state, address);

    if (address < 0x2000)
    {
        uint32_t chr_address;
        if (state->chr_bank_mode)
        {
            if (address < 0x400)        chr_address = state->banks[2] * 0x400 + address;
            else if (address < 0x800)   chr_address = state->banks[3] * 0x400 + (address - 0x400);
            else if (address < 0xC00)   chr_address = state->banks[4] * 0x400 + (address - 0x800);
            else if (address < 0x1000)  chr_address = state->banks[5] * 0x400 + (address - 0xC00);
            else if (address < 0x1800)  chr_address = state->banks[0] * 0x400 + (address - 0x1000);
            else                        chr_address = state->banks[1] * 0x400 + (address - 0x1800);
        }
        else
        {
            if (address < 0x800)        chr_address = state->banks[0] * 0x400 + address;
            else if (address < 0x1000)  chr_address = state->banks[1] * 0x400 + (address - 0x800);
            else if (address < 0x1400)  chr_address = state->banks[2] * 0x400 + (address - 0x1000);
            else if (address < 0x1800)  chr_address = state->banks[3] * 0x400 + (address - 0x1400);
            else if (address < 0x1C00)  chr_address = state->banks[4] * 0x400 + (address - 0x1800);
            else                        chr_address = state->banks[5] * 0x400 + (address - 0x1C00);
        }

        if (chr_address < cartridge->chr_rom_size)      *out_data = cartridge->chr_rom[chr_address];
        else if (chr_address < cartridge->chr_ram_size) *out_data = cartridge->chr_ram[chr_address];
    }
    else
    {
        if (state->nametable_4screen)
        {
            mmc3_mapper_state_4screen* state_4screen = (mmc3_mapper_state_4screen*)cartridge->state;
            address = ((address & 0x3FFF) - 0x2000);
            if (address < 0x1000)
                *out_data = state_4screen->nametable_vram[address];
            else
                *out_data = vram[address - 0x1000];
        }
        else
        {
            address = MMC3_nametable_address(state, address);
            *out_data = vram[address];
        }
    }
}

static void MMC3_ppu_write(nes_cartridge* cartridge, uint8_t* vram, uint16_t address, uint8_t data)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;

    MMC3_check_A12(state, address);

    if (address < 0x2000)
    {
        NROM_ppu_write(cartridge, vram, address, data);
    }
    else
    {
        if (state->nametable_4screen)
        {
            mmc3_mapper_state_4screen* state_4screen = (mmc3_mapper_state_4screen*)cartridge->state;
            address = ((address & 0x3FFF) - 0x2000);
            if (address < 0x1000)
                state_4screen->nametable_vram[address] = data;
            else
                vram[address - 0x1000] = data;
        }
        else
        {
            address = MMC3_nametable_address(state, address);
            vram[address] = data;
        }
    }
}

static void MMC3_tick(nes_cartridge* cartridge, cpu_state* cpu, nes_ppu* ppu)
{
    mmc3_mapper_state* state = (mmc3_mapper_state*)cartridge->state;

    cpu->irq |= state->has_irq;

    // Ignore dummy fetches
    if (_NES_PPU_IS_RENDERING(ppu) && ppu->vram_address == state->last_address)
        return;

    if (!MMC3_check_A12(state, ppu->vram_address) && state->a12_edge_counter)
        state->a12_edge_counter--;
}

static nes_mapper nes_mapper_get_MMC3(int layout_flag)
{
    nes_mapper mmc3rom = {sizeof(mmc3_mapper_state), &MMC3_init, &MMC3_read, &MMC3_write, &MMC3_ppu_read, &MMC3_ppu_write, &MMC3_tick};
    if (layout_flag)
    {
        mmc3rom.state_size = sizeof(mmc3_mapper_state_4screen);
        mmc3rom.init = &MMC3_init_4screen;
    }
    return mmc3rom;
}

// Select mapper

static nes_mapper nes_mapper_get(int mapper_id, int layout_flag)
{
    switch(mapper_id)
    {
        case 1:     return nes_mapper_get_MMC1();
        case 2:     return nes_mapper_get_UxROM();
        case 3:     return nes_mapper_get_CNROM();
        case 4:     return nes_mapper_get_MMC3(layout_flag);
        case 7:     return nes_mapper_get_AxROM();
        case 71:    return nes_mapper_get_Mapper071();
    }
    return nes_mapper_get_NROM();
}

#endif
