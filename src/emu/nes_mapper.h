#ifndef _NES_MAPPER_H_
#define _NES_MAPPER_H_

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "nes_cartridge.h"

typedef struct nes_mapper
{
    size_t  state_size;
    void    (*init)(nes_cartridge*);
    uint8_t (*read)(nes_cartridge*, uint16_t);
    void    (*write)(nes_cartridge*, uint16_t, uint8_t);
    uint8_t (*read_chr)(nes_cartridge*, uint16_t);
} nes_mapper;

// NROM

static void NROM_init(nes_cartridge* cartridge){}
static void NROM_write(nes_cartridge* cartridge, uint16_t address, uint8_t data){}

static uint8_t NROM_read(nes_cartridge* cartridge, uint16_t address)
{
    if ((cartridge->prg_rom_size/1024) == 32)
        return *(cartridge->prg_rom + (address & 0x7FFF));
    else
        return *(cartridge->prg_rom + (address & 0x3FFF));
}

static uint8_t NROM_read_chr(nes_cartridge* cartridge, uint16_t address)
{
    return *(cartridge->chr_rom + address);
}

static nes_mapper nes_mapper_get_NROM()
{
    nes_mapper nrom = {0, &NROM_init, &NROM_read, &NROM_write, &NROM_read_chr};
    return nrom;
}

// UxROM

typedef struct uxrom_mapper_state
{
    size_t  current_bank_offset;
    size_t  fixed_bank_offset;
    uint8_t bank_mask;
}uxrom_mapper_state;

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
    nes_mapper unrom = {sizeof(uxrom_mapper_state), &UxROM_init, &UxROM_read, &UxROM_write, &NROM_read_chr};
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
    nes_mapper unrom = {sizeof(cnrom_mapper_state), &CNROM_init, &NROM_read, &CNROM_write, &CNROM_read_chr};
    return unrom;
}

// MMC1

typedef struct mmc1_mapper_state
{
    uint8_t  bank_mode      : 2;
    uint8_t  chr_bank_mode  : 1;
    uint8_t  write_enable   : 1;
    uint8_t  ram_enable     : 1;
    size_t   fixed_bank_offset;
    size_t   current_bank_offset;
    size_t   chr_bank_low_offset;
    size_t   chr_bank_high_offset;
    uint8_t  shift_reg;
    uint8_t  shift_count;
    uint8_t  ram[8192];
}mmc1_mapper_state;

static void MMC1_init(nes_cartridge* cartridge)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    state->bank_mode = 3;
    state->chr_bank_mode = 0;
    state->fixed_bank_offset    = cartridge->prg_rom_size - (16 * 1024);
    state->current_bank_offset  = 0;
    state->chr_bank_low_offset  = 0;
    state->chr_bank_high_offset = 0;
    state->shift_reg = 0;
    state->shift_count = 0;
    state->write_enable = 1;
    state->ram_enable = 1;
}

static uint8_t MMC1_read(nes_cartridge* cartridge, uint16_t address)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    state->write_enable = 1;

    if (address >= 0x6000 && address <= 0x7FFF)
        return state->ram[address - 0x6000];

    switch (state->bank_mode)
    {
        default:
        case 0: case 1:
            return cartridge->prg_rom[state->current_bank_offset + (address - 0x8000)];
        case 2:
            if (address >= 0xC000)
                return cartridge->prg_rom[state->current_bank_offset + (address - 0xC000)];
            else
                return cartridge->prg_rom[state->fixed_bank_offset + (address - 0x8000)];
        case 3:
            if (address >= 0xC000)
                return cartridge->prg_rom[state->fixed_bank_offset + (address - 0xC000)];
            else
                return cartridge->prg_rom[state->current_bank_offset + (address - 0x8000)];
    }
}


static void MMC1_write(nes_cartridge* cartridge, uint16_t address, uint8_t data)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    if (address >= 0x6000 && address <= 0x7FFF)
    {
        state->write_enable = 1;
        state->ram[address - 0x6000] = data;
    }
    else
    {
        assert(state->write_enable);
        state->write_enable = 0;
        if (data & 0x80)
        {
            state->shift_reg = 0;
            state->shift_count = 0;
        }
        else if (++state->shift_count < 5)
        {
            state->shift_reg = ((data & 1) << 4) | (state->shift_reg >> 1);
        }
        else
        {
            data = ((data & 1) << 4) | (state->shift_reg >> 1);
            switch ((address >> 13) & 3)
            {
                case 0:
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
                            state->fixed_bank_offset = cartridge->prg_rom_size - (16 * 1024);
                        break;
                    }
                break;
                case 1:
                {
                    unsigned m = 1 - state->chr_bank_mode;
                    unsigned bank = (data & 0x1F) & ~m;
                    state->chr_bank_low_offset = bank * (4 * 1024);
                }
                break;
                case 2:
                    state->chr_bank_high_offset = data * (4 * 1024);
                break;
                case 3:
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

static uint8_t MMC1_read_chr(nes_cartridge* cartridge, uint16_t address)
{
    mmc1_mapper_state* state = (mmc1_mapper_state*)cartridge->mapper_state;
    state->write_enable = 1;
    if (state->chr_bank_mode == 0 || address < 0x1000)
        return cartridge->chr_rom[state->chr_bank_low_offset + address];
    else
        return cartridge->chr_rom[state->chr_bank_high_offset + (address - 0x1000)];
}

static nes_mapper nes_mapper_get_MMC1()
{
    nes_mapper unrom = {sizeof(mmc1_mapper_state), &MMC1_init, &MMC1_read, &MMC1_write, &MMC1_read_chr};
    return unrom;
}


static nes_mapper nes_mapper_get(int mapper_id)
{
    switch(mapper_id)
    {
        case 1: return nes_mapper_get_MMC1();
        case 2: return nes_mapper_get_UxROM();
        case 3: return nes_mapper_get_CNROM();
    }
    return nes_mapper_get_NROM();
}

#endif
