#ifndef _NES_ROM_H_
#define _NES_ROM_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>

#include "nes_cartridge.h"
#include "nes_mapper.h"

typedef enum nes_rom_format
{
    NES_ROM_FORMAT_UNKNOWN,
    NES_ROM_FORMAT_INES
} nes_rom_format;

struct ines_header
{
    uint8_t magic[4];
    uint8_t prg_rom_size;
    uint8_t chr_rom_size;
    union Flags6
    {
        struct
        {
            uint8_t mirroring : 1;
            uint8_t battery_ram : 1;
            uint8_t has_trainer : 1;
            uint8_t alternative_nametable_layout : 1;
            uint8_t mapper_low : 4;
        };
        uint8_t b;
    } flags6;
    union Flags7
    {
        struct
        {
            uint8_t vs_unisystem : 1;
            uint8_t play_choice_10 : 1;
            uint8_t version : 2;
            uint8_t mapper_high : 4;
        };
        uint8_t b;
    } flags7;
    uint8_t prg_ram_size;
    union Flags9
    {
        struct
        {
            uint8_t tv_system : 1;
            uint8_t reserved : 7;
        };
        uint8_t b;
    } flags9;
    union Flags10
    {
        struct
        {
            uint8_t tv_system : 2;
            uint8_t unused :2;
            uint8_t has_prg_ram : 1;
            uint8_t bus_conflicts : 1;
        };
        uint8_t b;
    } flags10;
    uint8_t reserved[5];
};

static nes_cartridge* nes_rom_create_ines_cartridge(const void* rom_file, size_t rom_file_size)
{
    struct ines_header* hdr = (struct ines_header*)rom_file;
    int mapper_id = ((hdr->flags7.mapper_high << 4) | hdr->flags6.mapper_low);

    uint32_t chr_ram_size = hdr->chr_rom_size ? 0 : 0x2000;

    size_t cartridge_size = sizeof(nes_cartridge) + sizeof(nes_mapper);
    cartridge_size += hdr->prg_rom_size * 0x4000;
    cartridge_size += hdr->chr_rom_size * 0x2000;
    cartridge_size += chr_ram_size;

    nes_mapper mapper = nes_mapper_get(mapper_id, hdr->flags6.alternative_nametable_layout);
    cartridge_size += mapper.state_size;

    printf("PRG ROM: %d KB \tCHR ROM: %d KB\n", ((hdr->prg_rom_size * 16384)/1024), ((hdr->chr_rom_size * 8192)/1024));
    printf("mapper: %d\n", mapper_id);

    nes_cartridge* cartridge = (nes_cartridge*)malloc(cartridge_size);
    cartridge->prg_rom_size = hdr->prg_rom_size * 0x4000;
    cartridge->chr_rom_size = hdr->chr_rom_size * 0x2000;
    cartridge->chr_ram_size = hdr->chr_rom_size ? 0 : 0x2000;
    cartridge->state_size = mapper.state_size + cartridge->chr_ram_size;
    cartridge->mapper = (nes_mapper*)((uint8_t*)cartridge + sizeof(nes_cartridge));
    cartridge->prg_rom = (uint8_t*)cartridge->mapper + sizeof(nes_mapper);
    cartridge->chr_rom = cartridge->prg_rom + cartridge->prg_rom_size;
    cartridge->state   = cartridge->chr_rom + cartridge->chr_rom_size;
    cartridge->chr_ram = (uint8_t*)cartridge->state + mapper.state_size;

    if (hdr->flags6.mirroring)
        cartridge->mirroring = NES_NAMETABLE_MIRRORING_VERTICAL;
    else
        cartridge->mirroring = NES_NAMETABLE_MIRRORING_HORIZONTAL;

    memcpy(cartridge->mapper, &mapper, sizeof(nes_mapper));
    mapper.init(cartridge);

    uint8_t* prg_ptr = (uint8_t*)rom_file + sizeof(struct ines_header) + (hdr->flags6.has_trainer?512:0);
    memcpy(cartridge->prg_rom, prg_ptr, cartridge->prg_rom_size);
    memcpy(cartridge->chr_rom, prg_ptr + cartridge->prg_rom_size, cartridge->chr_rom_size);

    return cartridge;
}

static nes_rom_format nes_rom_get_format(const void* rom_file, size_t rom_file_size)
{
    if (rom_file_size > 4 && memcmp(rom_file, "NES\x1a", 4) == 0)
        return NES_ROM_FORMAT_INES;

    return NES_ROM_FORMAT_UNKNOWN;
}

static nes_cartridge* nes_rom_create_cartridge(const void* rom_file, size_t rom_file_size)
{
    switch(nes_rom_get_format(rom_file, rom_file_size))
    {
        case NES_ROM_FORMAT_INES:    return nes_rom_create_ines_cartridge(rom_file, rom_file_size);
        case NES_ROM_FORMAT_UNKNOWN: return 0;
    }
    return 0;
}

static nes_cartridge* nes_rom_load_cartridge(const char* path)
{
    nes_cartridge* cartridge = 0;

#if _WIN32
    FILE* rom_file = NULL;
    if (fopen_s(&rom_file, path, "rb") == 0)
#else
    FILE* rom_file = fopen(path, "rb");
    if (rom_file)
#endif
    {
        void*   rom_file_data = 0;
        size_t  rom_file_size = 0; 

        fseek(rom_file, 0, SEEK_END);
        rom_file_size = ftell(rom_file);
        fseek(rom_file, 0, SEEK_SET);

        rom_file_data = malloc(rom_file_size);
        fread(rom_file_data, rom_file_size, 1, rom_file);

        cartridge = nes_rom_create_cartridge(rom_file_data, rom_file_size);
        free(rom_file_data);

        fclose(rom_file);
    }
    else
    {
        perror("Failed to load NES ROM");
    }

    return cartridge;
}

#endif
