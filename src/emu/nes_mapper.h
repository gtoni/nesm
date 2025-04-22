#ifndef _NES_MAPPER_H_
#define _NES_MAPPER_H_

#include <stdlib.h>
#include <stdint.h>

#include "nes_cartridge.h"
#include "emu6502.h"

typedef struct nes_mapper
{
    size_t      state_size;
    void        (*init)(nes_cartridge*);
    uint8_t     (*read)(nes_cartridge*, uint16_t);
    void        (*write)(nes_cartridge*, uint16_t, uint8_t);
    uint8_t     (*read_chr)(nes_cartridge*, uint16_t);
    void        (*tick)(nes_cartridge*, cpu_state*);
    uint16_t    (*get_nametable_address)(nes_cartridge*, uint16_t);
} nes_mapper;

// NROM

static void NROM_init(nes_cartridge* cartridge){}
static void NROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data){}

static uint8_t NROM_read(nes_cartridge* cartridge, uint16_t address)
{
    if (cartridge->prg_rom_size == 0x8000)
        return *(cartridge->prg_rom + (address & 0x7FFF));
    else
        return *(cartridge->prg_rom + (address & 0x3FFF));
}

static uint8_t NROM_read_chr(nes_cartridge* cartridge, uint16_t address)
{
    return *(cartridge->chr_rom + address);
}

static void NROM_tick(nes_cartridge* cartridge, cpu_state* cpu) {}

static uint16_t NROM_nametable_address(nes_cartridge* cartridge, uint16_t address)
{
    switch (cartridge->mirroring)
    {
        case NES_NAMETABLE_MIRRORING_VERTICAL:      address = address & 0x7FF; break;
        case NES_NAMETABLE_MIRRORING_HORIZONTAL:    address = ((address / 2) & 0x400) + (address & 0x3FF); break;
        case NES_NAMETABLE_MIRRORING_SINGLE_LOW:    address = address & 0x3FF; break;
        case NES_NAMETABLE_MIRRORING_SINGLE_HIGH:   address = 0x800 + (address & 0x3FF); break;
    }
    return address;
}

static nes_mapper nes_mapper_get_NROM()
{
    nes_mapper nrom = {0, &NROM_init, &NROM_read, &NROM_write, &NROM_read_chr, &NROM_tick, &NROM_nametable_address};
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
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->mapper_state;
    size_t num_banks = cartridge->prg_rom_size / 0x8000;
    state->bank_offset = (num_banks - 1) * 0x8000; 
    state->mirroring = 0;
}

static uint8_t AxROM_read(nes_cartridge* cartridge, uint16_t address)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->mapper_state;
    return cartridge->prg_rom[(state->bank_offset + (address - 0x8000)) % cartridge->prg_rom_size];
}

static void AxROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->mapper_state;
    state->bank_offset = (data & 7) * 0x8000;
    state->mirroring = ((uint16_t)data & 0x10) << 7;
}

static uint16_t AxROM_nametable_address(nes_cartridge* cartridge, uint16_t address)
{
    axrom_mapper_state* state = (axrom_mapper_state*)cartridge->mapper_state;
    return state->mirroring + (address & 0x3FF);
}

static nes_mapper nes_mapper_get_AxROM()
{
    nes_mapper unrom = {sizeof(axrom_mapper_state), &AxROM_init, &AxROM_read, &AxROM_write, &NROM_read_chr, &NROM_tick, &AxROM_nametable_address};
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
    const size_t bank_size = 16 * 1024;
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->mapper_state;
    size_t num_banks = cartridge->prg_rom_size / bank_size;
    state->fixed_bank_offset = (num_banks - 1) * bank_size; 
    state->current_bank_offset = state->fixed_bank_offset;
    if (num_banks < 16)
        state->bank_mask = 0x07;
    else
        state->bank_mask = 0x0F;
}

static uint8_t UxROM_read(nes_cartridge* cartridge, uint16_t address)
{
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->mapper_state;
    if (address >= 0xC000)
        return cartridge->prg_rom[state->fixed_bank_offset + (address - 0xC000)];
    else
        return cartridge->prg_rom[state->current_bank_offset + (address - 0x8000)];
}

static void UxROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    const size_t bank_size = 16 * 1024;
    uxrom_mapper_state* state = (uxrom_mapper_state*)cartridge->mapper_state;
    if (UxROM_read(cartridge, address) == data)
    {
        state->current_bank_offset = (data & state->bank_mask) * bank_size;
    }
}

static nes_mapper nes_mapper_get_UxROM()
{
    nes_mapper unrom = {sizeof(uxrom_mapper_state), &UxROM_init, &UxROM_read, &UxROM_write, &NROM_read_chr, &NROM_tick, &NROM_nametable_address};
    return unrom;
}

// CNROM

typedef struct cnrom_mapper_state
{
    size_t  current_bank_offset;
    uint8_t bank_mask;
}cnrom_mapper_state;

static void CNROM_init(nes_cartridge* cartridge)
{
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->mapper_state;
    state->current_bank_offset = 0;
    state->bank_mask = 0x3;
}

static void CNROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    const size_t bank_size = 8 * 1024;
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->mapper_state;
    state->current_bank_offset = (data & state->bank_mask) * bank_size;
}

static uint8_t CNROM_read_chr(nes_cartridge* cartridge, uint16_t address)
{
    cnrom_mapper_state* state = (cnrom_mapper_state*)cartridge->mapper_state;
    return cartridge->chr_rom[state->current_bank_offset + address];
}

static nes_mapper nes_mapper_get_CNROM()
{
    nes_mapper unrom = {sizeof(cnrom_mapper_state), &CNROM_init, &NROM_read, &CNROM_write, &CNROM_read_chr, &NROM_tick, &NROM_nametable_address};
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
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    state->bank_mode = 3;
    state->chr_bank_mode = 0;
    state->prg_bank_selector    = 0;
    state->fixed_bank_offset    = (cartridge->prg_rom_size - (16 * 1024)) & 0x3FFFF;
    state->current_bank_offset  = 0;
    state->chr_bank_low_offset  = 0;
    state->chr_bank_high_offset = 0;
    state->shift_reg = 0;
    state->shift_count = 0;
    state->write_enable = 1;
    state->ram_enable = 1;
    state->is_SUROM = (cartridge->prg_rom_size == 512 * 1024);
}

static uint8_t MMC1_read(nes_cartridge* cartridge, uint16_t address)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;

    if (address >= 0x6000 && address <= 0x7FFF)
        return state->ram[address - 0x6000];

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

    return cartridge->prg_rom[prg_address];
}


static void MMC1_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
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
            state->fixed_bank_offset = (cartridge->prg_rom_size - (16 * 1024)) & 0x3FFFF;
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
                                state->fixed_bank_offset = (cartridge->prg_rom_size - (16 * 1024)) & 0x3FFFF;
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
                        state->current_bank_offset = bank * (16 * 1024);
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

static uint8_t MMC1_read_chr(nes_cartridge* cartridge, uint16_t address)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    if (state->chr_bank_mode == 0 || address < 0x1000)
        return cartridge->chr_rom[state->chr_bank_low_offset + address];
    else
        return cartridge->chr_rom[state->chr_bank_high_offset + (address - 0x1000)];
}

static void MMC1_tick(nes_cartridge* cartridge, cpu_state* cpu)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    if ((cpu->cycle & 0xFF) == 0)
        state->write_enable = 1;
}

static nes_mapper nes_mapper_get_MMC1()
{
    nes_mapper unrom = {sizeof(mmc1_mapper_state), &MMC1_init, &MMC1_read, &MMC1_write, &MMC1_read_chr, &MMC1_tick, &NROM_nametable_address};
    return unrom;
}

// Select mapper

static nes_mapper nes_mapper_get(int mapper_id)
{
    switch(mapper_id)
    {
        case 1: return nes_mapper_get_MMC1();
        case 2: return nes_mapper_get_UxROM();
        case 3: return nes_mapper_get_CNROM();
        case 7: return nes_mapper_get_AxROM();
    }
    return nes_mapper_get_NROM();
}

#endif
