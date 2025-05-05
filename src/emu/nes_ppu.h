#ifndef _PPU_H_
#define _PPU_H_

#include <memory.h>
#include <stdint.h>
#include <assert.h>

#define SCANLINE_WIDTH          341
#define TOTAL_SCANLINES         262

#define PRE_RENDER_SCANLINE     261
#define RENDER_BEGIN_SCANLINE   0
#define RENDER_END_SCANLINE     240
#define VBLANK_BEGIN_SCANLINE   241
#define VBLANK_END_SCANLINE     261

enum NES_PPU_REG_RW_MODE 
{ 
    NES_PPU_REG_RW_MODE_NONE,
    NES_PPU_REG_RW_MODE_READ,
    NES_PPU_REG_RW_MODE_WRITE
};

#define NES_PPU_CTRL_REG_ID         0
#define NES_PPU_MASK_REG_ID         1
#define NES_PPU_STATUS_REG_ID       2
#define NES_PPU_OAM_ADDR_REG_ID     3
#define NES_PPU_OAM_DATA_REG_ID     4
#define NES_PPU_SCROLL_REG_ID       5
#define NES_PPU_ADDR_REG_ID         6
#define NES_PPU_DATA_REG_ID         7

typedef union nes_ppu_ctrl_reg
{
    struct
    {
        uint8_t base_nametable_addr         : 2;
        uint8_t vram_rw_increment_step      : 1;
        uint8_t sprite_pattern_table_addr   : 1;
        uint8_t bgr_pattern_table_addr      : 1;
        uint8_t sprite_size                 : 1;
        uint8_t master_slave_select         : 1;
        uint8_t generate_nmi                : 1;
    };
    uint8_t b;
} nes_ppu_ctrl_reg;

typedef enum nes_ppu_render_mask
{
    NES_PPU_RENDER_MASK_GRAYSCALE           = 0x01,
    NES_PPU_RENDER_MASK_LEFTMOST_BACKGROUND = 0x02,
    NES_PPU_RENDER_MASK_LEFTMOST_SPRITES    = 0x04,
    NES_PPU_RENDER_MASK_BACKGROUND          = 0x08,
    NES_PPU_RENDER_MASK_SPRITES             = 0x10,
    NES_PPU_RENDER_MASK_EMPHASIZE_RED       = 0x20,
    NES_PPU_RENDER_MASK_EMPHASIZE_GREEN     = 0x40,
    NES_PPU_RENDER_MASK_EMPHASIZE_BLUE      = 0x80,

    NES_PPU_RENDER_MASK_RENDER           = NES_PPU_RENDER_MASK_BACKGROUND | NES_PPU_RENDER_MASK_SPRITES,
    NES_PPU_RENDER_MASK_RENDER_LEFTMOST  = NES_PPU_RENDER_MASK_LEFTMOST_BACKGROUND | NES_PPU_RENDER_MASK_LEFTMOST_SPRITES
} nes_ppu_render_mask;

typedef union nes_ppu_status_reg
{
    struct
    {
        uint8_t low : 5;
        uint8_t sprite_overflow : 1;
        uint8_t sprite_0_hit : 1;
        uint8_t vblank_started : 1; 
    };
    uint8_t b;
} nes_ppu_status_reg;

typedef struct nes_ppu_vaddr
{
    uint16_t coarse_x : 5;
    uint16_t coarse_y : 5;
    uint16_t nametable : 2;
    uint16_t fine_y : 3;
    uint16_t unused : 1;
} nes_ppu_vaddr;

typedef union nes_ppu_sprite_attrib
{
   struct
   {
       uint8_t palette : 2;
       uint8_t unused : 3;
       uint8_t priority : 1;
       uint8_t flip_x : 1;
       uint8_t flip_y : 1;
   };
   uint8_t b;
} nes_ppu_sprite_attrib;

typedef struct nes_ppu_oam_entry
{
   uint8_t                  position_y;
   uint8_t                  tile_index;
   nes_ppu_sprite_attrib    attribute;
   uint8_t                  position_x;
} nes_ppu_oam_entry;

typedef struct nes_ppu
{
    uint32_t dot;
    uint32_t scanline;

    uint8_t reg_rw_mode;
    uint8_t reg_data;
    uint8_t reg_addr    : 3;
    uint8_t r           : 1;
    uint8_t w           : 1;
    uint8_t vbl         : 1;

    uint16_t color_out;

    unsigned is_even_frame;

    union
    {
        nes_ppu_vaddr   v_addr;
        uint16_t        v_addr_reg;
    };

    union
    {
        nes_ppu_vaddr   t_addr;
        uint16_t        t_addr_reg;
    };

    int      write_toggle;
    uint8_t  fine_x;

    uint8_t  tile_value;
    uint8_t  palette_attribute;
    uint8_t  bitplane_slice_low;

    uint16_t bg_shift_low;
    uint16_t bg_shift_high;
    uint16_t attr_shift_low;
    uint16_t attr_shift_high;

    nes_ppu_sprite_attrib   sprite_attributes[8];
    uint8_t                 sprite_x_positions[8];
    uint8_t                 sprite_shift_low[8];
    uint8_t                 sprite_shift_high[8];

    uint8_t         eval_oam_has_sprite_zero;
    uint8_t         eval_oam_entry_data;
    uint8_t         eval_oam_byte_count;
    uint8_t         eval_oam_src_addr;

    uint8_t         sprite_0_test;
    uint8_t         cpu_read_buffer;
    int             update_cpu_read_buffer;

    uint8_t         pre_vblank;

    nes_ppu_ctrl_reg     ctrl;
    nes_ppu_render_mask  render_mask;
    nes_ppu_render_mask  next_render_mask;
    nes_ppu_status_reg   status;
    uint8_t              oam_address;
    uint16_t             vram_address;
    uint8_t              vram_data;

    union oam_memory
    {
        nes_ppu_oam_entry   entries[64];
        uint8_t             bytes[64*4];
    } primary_oam;

    union oam_memory2
    {
        nes_ppu_oam_entry   entries[8];
        uint8_t             bytes[8*4];
    } secondary_oam;

    uint8_t         palettes[32];

    uint8_t         open_bus;
    uint8_t         open_bus_decay_timer[8];
} nes_ppu;

#define _NES_PPU_IS_RENDERING(ppu) ((ppu->scanline < RENDER_END_SCANLINE || ppu->scanline == PRE_RENDER_SCANLINE) &&\
                                    (ppu->render_mask & NES_PPU_RENDER_MASK_RENDER))

static void nes_ppu_reset(nes_ppu* ppu)
{
    uint8_t default_palette[] = {
        0x09,0x01,0x00,0x01,0x00,0x02,0x02,0x0D,0x08,0x10,0x08,0x24,0x00,0x00,0x04,0x2C,
        0x09,0x01,0x34,0x03,0x00,0x04,0x00,0x14,0x08,0x3A,0x00,0x02,0x00,0x20,0x2C,0x08
    };
    memset(ppu, 0, sizeof(nes_ppu));
    memcpy(ppu->palettes, default_palette, sizeof(default_palette));
    ppu->scanline = VBLANK_BEGIN_SCANLINE;
    ppu->color_out = 0x0F;
}

static void nes_ppu_execute(nes_ppu* __restrict ppu)
{
    uint8_t palette_index = 0;
    nes_ppu_render_mask next_render_mask = ppu->next_render_mask;

    // Update dot and scanline
    if (++ppu->dot >= SCANLINE_WIDTH)
    {
        ppu->scanline = (ppu->scanline + 1) % TOTAL_SCANLINES;
        ppu->dot = 0;
    }

    // VBlank

    ppu->vbl = ppu->status.vblank_started && ppu->ctrl.generate_nmi;

    if (ppu->scanline == VBLANK_BEGIN_SCANLINE)
    {
        if (ppu->dot == 0)      ppu->pre_vblank = 1;
        else if (ppu->dot == 1) ppu->status.vblank_started = ppu->pre_vblank;
    }
    else if (ppu->scanline == PRE_RENDER_SCANLINE && ppu->dot == 1)
    {
        // Clear VBlank, Sprite 0 Hit, Sprite overflow and toggle odd/even flag
        ppu->status.vblank_started = 0;
        ppu->status.sprite_0_hit = 0;
        ppu->status.sprite_overflow = 0;
        ppu->is_even_frame = !ppu->is_even_frame;

        // open bus decay
        uint8_t open_bus_mask = 0;
        for (uint32_t i = 0; i < 8; ++i)
            open_bus_mask |= (ppu->open_bus_decay_timer[i]++ < 36) << i;    // 36 * 16.6ms = ~600ms

        ppu->open_bus &= open_bus_mask;
    }

    // I/O:

    if (ppu->r | ppu->w)
    {
        if (!_NES_PPU_IS_RENDERING(ppu))
            ppu->vram_address += ppu->ctrl.vram_rw_increment_step ? 32 : 1;

        if (ppu->update_cpu_read_buffer)
        {
            ppu->cpu_read_buffer = ppu->vram_data;
            ppu->update_cpu_read_buffer = 0;
        }
        ppu->r = ppu->w = 0;
    }

    if (ppu->vram_address >= 0x3F00 && ppu->vram_address <= 0x3FFF)
    {
        palette_index = ppu->vram_address & 0x1F;
        if ((palette_index & 0x13) == 0x10)
            palette_index &= ~0x10;
    }

    if (ppu->reg_rw_mode)
    {
        uint8_t open_bus_refresh_bits = 0;

        if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_READ)
            ppu->reg_data = ppu->open_bus;
        else
            open_bus_refresh_bits = 0xFF;

        switch(ppu->reg_addr)
        {
            case NES_PPU_CTRL_REG_ID:   // write-only 
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                {
                    ppu->ctrl.b = ppu->reg_data;
                    ppu->t_addr.nametable = ppu->ctrl.base_nametable_addr;
                }
            }
            break;
            case NES_PPU_MASK_REG_ID:   // write-only
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                    next_render_mask = (nes_ppu_render_mask)ppu->reg_data;
            }
            break;
            case NES_PPU_STATUS_REG_ID: // read-only
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_READ)
                {
                    ppu->reg_data = (ppu->status.b & 0xE0) | (ppu->open_bus & 0x1F);
                    ppu->status.vblank_started = 0;
                    ppu->write_toggle = 0;
                    ppu->pre_vblank = 0;
                    open_bus_refresh_bits = 0xE0;
                }
            }
            break;
            case NES_PPU_OAM_ADDR_REG_ID: // write-only
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                    ppu->oam_address = ppu->reg_data;
            }
            break;
            case NES_PPU_OAM_DATA_REG_ID: // read-write
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                {
                    // sprite attribute bits 2-4 are always 0
                    ppu->primary_oam.bytes[ppu->oam_address] = (ppu->oam_address & 3) == 2 ? ppu->reg_data & 0xE3 : ppu->reg_data;
                    ++ppu->oam_address;
                }
                else
                {
                    ppu->reg_data = ppu->primary_oam.bytes[ppu->oam_address];
                    open_bus_refresh_bits = 0xFF;
                }
            }
            break;
            case NES_PPU_SCROLL_REG_ID: // write-only
            {
                if(ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                {
                    if(ppu->write_toggle)
                    {
                        ppu->t_addr.coarse_y = ppu->reg_data >> 3;
                        ppu->t_addr.fine_y = ppu->reg_data & 7;
                    }
                    else
                    {
                        ppu->t_addr.coarse_x = ppu->reg_data >> 3;
                        ppu->fine_x = ppu->reg_data & 7;
                    }
                    ppu->write_toggle = !ppu->write_toggle; 
                }
            }
            break;
            case NES_PPU_ADDR_REG_ID: // write-only
            {
                if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                {
                    if (ppu->write_toggle)
                    {
                        ppu->t_addr_reg = (ppu->t_addr_reg & 0xFF00) | ppu->reg_data;
                        ppu->v_addr_reg = ppu->t_addr_reg;
                        ppu->vram_address = ppu->t_addr_reg;
                    }
                    else
                    {
                        ppu->t_addr_reg = ((ppu->reg_data & 0x3F) << 8) | (ppu->vram_address & 0x00FF);
                    }
                    ppu->write_toggle = !ppu->write_toggle;
                }
            }
            break;
            case NES_PPU_DATA_REG_ID: // read-write
            {
                if (ppu->vram_address >= 0x3F00 && ppu->vram_address <= 0x3FFF)
                {
                    if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                    {
                        ppu->palettes[palette_index] = ppu->reg_data & 0x3F;

                        if (!_NES_PPU_IS_RENDERING(ppu))
                            ppu->vram_address += ppu->ctrl.vram_rw_increment_step ? 32 : 1;
                    }
                    else
                    {
                        ppu->r = 1;
                        ppu->reg_data = (ppu->palettes[palette_index] & 0x3F) | (ppu->open_bus & 0xC0);
                        ppu->update_cpu_read_buffer = 1;
                        open_bus_refresh_bits = 0x3F;
                    }
                }
                else
                {
                    if (ppu->reg_rw_mode == NES_PPU_REG_RW_MODE_WRITE)
                    {
                        ppu->w = 1;
                        ppu->vram_data = ppu->reg_data;
                    }
                    else
                    {
                        ppu->r = 1;
                        ppu->reg_data = ppu->cpu_read_buffer;
                        ppu->update_cpu_read_buffer = 1;
                        open_bus_refresh_bits = 0xFF;
                    }
                }

                // If rendering immediately increment X and Y scroll
                if (_NES_PPU_IS_RENDERING(ppu))
                {
                    if (ppu->v_addr.coarse_x == 31)
                    {
                        ppu->v_addr.nametable ^= 1;
                        ppu->v_addr.coarse_x = 0;
                    }
                    else
                    {
                        ppu->v_addr.coarse_x++;
                    }

                    if (ppu->v_addr.fine_y < 7)
                    {
                        ppu->v_addr.fine_y++;
                    }
                    else
                    {
                        ppu->v_addr.fine_y = 0;
                        if (ppu->v_addr.coarse_y == 29)
                        {
                            ppu->v_addr.coarse_y = 0;
                            ppu->v_addr.nametable ^= 2;
                        }
                        else if (ppu->v_addr.coarse_y == 31)
                        {
                            ppu->v_addr.coarse_y = 0;
                        }
                        else
                        {
                            ppu->v_addr.coarse_y++;
                        }
                    }
                }
            }
            break;
        }

        if (open_bus_refresh_bits)
        {
            ppu->open_bus = ppu->reg_data;

            for (uint32_t i = 0; i < 8; ++i)
            {
                uint8_t bit_mask = 1 << i;
                if (open_bus_refresh_bits & bit_mask)
                    ppu->open_bus_decay_timer[i] = 0;
            }
        }

        ppu->reg_rw_mode = NES_PPU_REG_RW_MODE_NONE;
    }

    // Rendering

    int is_render_enabled      = (ppu->render_mask & NES_PPU_RENDER_MASK_RENDER) != 0;
    int is_next_render_enabled = (next_render_mask & NES_PPU_RENDER_MASK_RENDER) != 0;

    if (is_render_enabled == is_next_render_enabled)
    {
        ppu->render_mask = ppu->next_render_mask = next_render_mask;
    }
    else
    {
        ppu->render_mask = (nes_ppu_render_mask)((ppu->render_mask & NES_PPU_RENDER_MASK_RENDER) | (next_render_mask & ~NES_PPU_RENDER_MASK_RENDER));
    }

    if (_NES_PPU_IS_RENDERING(ppu))
    {
        const unsigned sprite_height = 8 << ppu->ctrl.sprite_size;

        palette_index = 0;

        // Draw pixel
        if (ppu->dot <= 257)
        {
            int x = ppu->dot - 2;
            unsigned bg_pattern = 0;
            
            if ((ppu->render_mask & NES_PPU_RENDER_MASK_BACKGROUND) && 
               ((ppu->render_mask & NES_PPU_RENDER_MASK_LEFTMOST_BACKGROUND) || x > 7))
            {
                unsigned bg_shift_x = 15 - ppu->fine_x;
                bg_pattern = ((ppu->bg_shift_low  >> (bg_shift_x))     & 0x01) |
                             ((ppu->bg_shift_high >> (bg_shift_x - 1)) & 0x02);

                if (bg_pattern)
                {
                    palette_index = (bg_pattern |
                                ((ppu->attr_shift_low  >> (bg_shift_x - 2)) & 0x04) | 
                                ((ppu->attr_shift_high >> (bg_shift_x - 3)) & 0x08)) & 0x0F;
                }
            }

            if ((ppu->render_mask & NES_PPU_RENDER_MASK_SPRITES) && 
               ((ppu->render_mask & NES_PPU_RENDER_MASK_LEFTMOST_SPRITES) || x > 7))
            {
                for (int i = 0; i < 8; ++i)
                {
                    if (x >= ppu->sprite_x_positions[i] && (x < ppu->sprite_x_positions[i] + 8))
                    {
                        unsigned sprite_shift = 7 - (x - ppu->sprite_x_positions[i]) & 7;
                        unsigned pattern = (((ppu->sprite_shift_high[i] >> sprite_shift) << 1) & 2) |
                                           ((ppu->sprite_shift_low[i] >> sprite_shift) & 1);

                        if (pattern)
                        {
                            if (i == 0 && ppu->sprite_0_test && ppu->status.sprite_0_hit == 0)
                                ppu->status.sprite_0_hit = bg_pattern && x != 255;

                            if (bg_pattern == 0 || (ppu->sprite_attributes[i].priority == 0))
                                palette_index = pattern | (ppu->sprite_attributes[i].palette << 2) | 0x10;

                            break;
                        }
                    }
                }
            }
        }

        // Shift background shift registers
        if ((ppu->dot >= 2 && ppu->dot <= 257) || (ppu->dot >= 322 && ppu->dot <= 337))
        {
            ppu->bg_shift_high <<= 1;
            ppu->bg_shift_low <<= 1;
            ppu->attr_shift_high <<= 1;
            ppu->attr_shift_low <<= 1;
        }

        if (ppu->dot && (ppu->dot <= 256 || (ppu->dot > 320 && ppu->dot <= 336)))
        {
            // Fetch background data and update shift registers
            switch(ppu->dot & 7)
            {
                case 1:
                    // Reload shift registers
                    ppu->bg_shift_high |= ppu->vram_data;
                    ppu->bg_shift_low |= ppu->bitplane_slice_low;
                    ppu->attr_shift_low |= (ppu->palette_attribute & 1) * 0xFF;
                    ppu->attr_shift_high |= ((ppu->palette_attribute >> 1) & 1) * 0xFF;

                    // Fetch tile from nametable
                    ppu->r = 1;
                    ppu->vram_address = 0x2000 | (ppu->v_addr_reg & 0x0FFF);
                    break;
                case 2:
                    ppu->tile_value = ppu->vram_data;
                    break;
                case 3:
                    // Fetch tile attributes
                    ppu->r = 1;
                    ppu->vram_address = 0x23C0 | (ppu->v_addr_reg & 0x0C00) | ((ppu->v_addr_reg >> 4) & 0x38) | ((ppu->v_addr_reg >> 2) & 0x07);
                    break;
                case 4:
                    ppu->palette_attribute = ppu->vram_data;
                    if (ppu->v_addr.coarse_y & 2) ppu->palette_attribute >>= 4;
                    if (ppu->v_addr.coarse_x & 2) ppu->palette_attribute >>= 2;
                    break;
                case 5:
                    // Fetch low bitplane
                    ppu->r = 1;
                    ppu->vram_address = (ppu->ctrl.bgr_pattern_table_addr << 12) | (ppu->tile_value << 4) | ppu->v_addr.fine_y;
                    break; 
                case 6:
                    ppu->bitplane_slice_low = ppu->vram_data;
                    break;
                case 7:
                    // Fetch high bitplane
                    ppu->r = 1;
                    ppu->vram_address = (ppu->ctrl.bgr_pattern_table_addr << 12) | (ppu->tile_value << 4) | 8 | ppu->v_addr.fine_y;
                    break;
                case 0:
                    // Increment horizontal scrolling
                    if (ppu->v_addr.coarse_x == 31)
                    {
                        ppu->v_addr.nametable ^= 1;
                        ppu->v_addr.coarse_x = 0;
                    }
                    else
                    {
                        ppu->v_addr.coarse_x++;
                    }
            
                    // Increment vertical scrolling
                    if (ppu->dot == 256)
                    {
                        if (ppu->v_addr.fine_y < 7)
                        {
                            ppu->v_addr.fine_y++;
                        }
                        else
                        {
                            ppu->v_addr.fine_y = 0;
                            if (ppu->v_addr.coarse_y == 29)
                            {
                                ppu->v_addr.coarse_y = 0;
                                ppu->v_addr.nametable ^= 2;
                            }
                            else if (ppu->v_addr.coarse_y == 31)
                            {
                                ppu->v_addr.coarse_y = 0;
                            }
                            else
                            {
                                ppu->v_addr.coarse_y++;
                            }
                        }
                    }
                    break;
            }

            // Reset OAM counters at dot 1 of each scanline
            if (ppu->dot == 1)
            {
                ppu->eval_oam_has_sprite_zero = 0;
                ppu->eval_oam_byte_count = 0;
                ppu->eval_oam_src_addr = 0;
            }

            // Sprite evaluation
            if (ppu->scanline >= RENDER_BEGIN_SCANLINE && ppu->dot < 257)
            {
                if (ppu->dot < 65)
                {
                    // clear secondary oam 
                    ppu->secondary_oam.bytes[(ppu->dot - 1) >> 1] = 0xFF;
                }
                else 
                {
                    if (ppu->dot & 1)
                    {
                        ppu->eval_oam_entry_data = ppu->primary_oam.bytes[ppu->oam_address];
                    }
                    else
                    {
                        if (ppu->oam_address >= ppu->eval_oam_src_addr)
                        {
                            ppu->eval_oam_src_addr = ppu->oam_address;

                            int copy_oam = 1;

                            if ((ppu->eval_oam_byte_count & 3) == 0)
                            {
                                uint8_t pos = ppu->eval_oam_entry_data;
                                if (pos < 240 && ppu->scanline >= pos && ppu->scanline < (pos + sprite_height))
                                {
                                    if (ppu->dot == 66)
                                        ppu->eval_oam_has_sprite_zero = 1;

                                    if (ppu->eval_oam_byte_count == 32)
                                        ppu->status.sprite_overflow = 1;
                                }
                                else
                                {
                                    copy_oam = 0;
                                }
                            }

                            if (copy_oam)
                            {
                                if (ppu->eval_oam_byte_count < 32)
                                    ppu->secondary_oam.bytes[ppu->eval_oam_byte_count++] = ppu->eval_oam_entry_data;

                                ppu->oam_address++;
                            }
                            else
                            {
                                ppu->oam_address = (ppu->oam_address + 4);

                                if (ppu->eval_oam_byte_count == 32)
                                    ppu->oam_address = (ppu->oam_address & 0xFC) | ((ppu->oam_address + 1) & 3);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // Update horizontal scrolling component from T
            if (ppu->dot == 257)
                ppu->v_addr_reg = (ppu->v_addr_reg & ~0x041F) | (ppu->t_addr_reg & 0x041F);

            // Fetch sprite data
            if ((ppu->render_mask & NES_PPU_RENDER_MASK_SPRITES) && ppu->dot && ppu->dot <= 320)
            {
                uint32_t            current_oam_index = (ppu->dot - 257) >> 3;
                uint32_t            max_oam_index = ppu->eval_oam_byte_count >> 2;
                nes_ppu_oam_entry   current_oam = ppu->secondary_oam.entries[current_oam_index];

                ppu->oam_address = 0;
                ppu->sprite_0_test = ppu->eval_oam_has_sprite_zero;

                switch(ppu->dot & 7)
                {
                    case 1: 
                        ppu->sprite_attributes[current_oam_index] = current_oam.attribute;
                        break;
                    case 2:
                        ppu->sprite_x_positions[current_oam_index] = current_oam.position_x;
                        break;
                    case 3:
                        break;
                    case 4:
                        break;
                    case 5:
                        {
                            uint16_t pattern_table = ppu->ctrl.sprite_pattern_table_addr;
                            unsigned tile_index = current_oam.tile_index;
                            if (ppu->ctrl.sprite_size)
                            {
                                pattern_table = current_oam.tile_index & 1;
                                tile_index = tile_index & ~1;
                            }

                            unsigned pos_y = ppu->scanline - current_oam.position_y;

                            if (current_oam.attribute.flip_y)
                                pos_y = sprite_height - pos_y - 1;

                            pos_y += pos_y & 8;
                            
                            ppu->r = 1;
                            ppu->vram_address = (pattern_table << 12) | (tile_index << 4) | pos_y;
                        }
                        break; 
                    case 6:
                        if (current_oam.attribute.flip_x)
                            ppu->sprite_shift_low[current_oam_index] = (uint8_t)(((ppu->vram_data * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32);
                        else
                            ppu->sprite_shift_low[current_oam_index] = ppu->vram_data;
                        break;
                    case 7:
                        {
                            uint16_t pattern_table = ppu->ctrl.sprite_pattern_table_addr;
                            unsigned tile_index = current_oam.tile_index;
                            if (ppu->ctrl.sprite_size)
                            {
                                pattern_table = current_oam.tile_index & 1;
                                tile_index = tile_index & ~1;
                            }

                            unsigned pos_y = ppu->scanline - current_oam.position_y;

                            if (current_oam.attribute.flip_y)
                                pos_y = sprite_height - pos_y - 1;

                            pos_y += pos_y & 8;
                            
                            ppu->r = 1;
                            ppu->vram_address = (pattern_table << 12) | (tile_index << 4) | (pos_y + 8);
                        }
                        break;
                    case 0:
                        if (current_oam.attribute.flip_x)
                            ppu->sprite_shift_high[current_oam_index] = (uint8_t)(((ppu->vram_data * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32);
                        else
                            ppu->sprite_shift_high[current_oam_index] = ppu->vram_data;

                        // Set sprite to transparent if it's not detected on scanline.
                        if (current_oam_index >= max_oam_index)
                        {
                            ppu->sprite_shift_high[current_oam_index] = 0;
                            ppu->sprite_shift_low[current_oam_index] = 0;
                        }
                        break;
                }
            }
       
            if (ppu->scanline == PRE_RENDER_SCANLINE)
            {
                if (ppu->dot >= 280 && ppu->dot <= 304)
                {
                    // Update vertical scrolling component from T during pre-render scanline
                    ppu->v_addr_reg = (ppu->v_addr_reg & ~0x7BE0) | (ppu->t_addr_reg & 0x7BE0);
                }
                else if (ppu->dot == 339 && !ppu->is_even_frame)
                {
                    // Skip one cycle if odd frame
                    ppu->dot = 340;
                }
            }
        }
    }

    ppu->color_out = ppu->palettes[palette_index];

    if (ppu->render_mask & NES_PPU_RENDER_MASK_GRAYSCALE)
        ppu->color_out &= 0x30;

    ppu->color_out |= (((uint16_t)ppu->render_mask) << 1) & 0x1C0;

    ppu->render_mask = ppu->next_render_mask;
    ppu->next_render_mask = next_render_mask;
}

#endif
