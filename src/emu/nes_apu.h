#ifndef _NES_APU_H_
#define _NES_APU_H_

#include <stdint.h>
#include <memory.h>
#include <assert.h>

#define NES_APU_PULSE1_REG0_ID          0x4000
#define NES_APU_PULSE1_REG1_ID          0x4001
#define NES_APU_PULSE1_REG2_ID          0x4002
#define NES_APU_PULSE1_REG3_ID          0x4003

#define NES_APU_PULSE2_REG0_ID          0x4004
#define NES_APU_PULSE2_REG1_ID          0x4005
#define NES_APU_PULSE2_REG2_ID          0x4006
#define NES_APU_PULSE2_REG3_ID          0x4007

#define NES_APU_STATUS_REG_ID           0x4015
#define NES_APU_FRAME_COUNTER_REG_ID    0x4017

enum NES_APU_REG_RW_MODE
{
    NES_APU_REG_RW_MODE_NONE,
    NES_APU_REG_RW_MODE_READ,
    NES_APU_REG_RW_MODE_WRITE
};

typedef struct nes_apu_pulse
{
    uint8_t     duty : 2;
    uint8_t     sequence_output : 1;
    union
    {
        uint8_t     length_counter_halt : 1;
        uint8_t     env_loop : 1;
    };
    uint8_t     env_start : 1;
    uint8_t     constant_volume : 1;
    uint8_t     sweep_enable : 1;
    uint8_t     sweep_negate : 1;
    uint8_t     sweep_reload : 1;
    uint16_t    timer;
    uint16_t    t;
    uint8_t     sequencer;
    uint8_t     length_counter;
    uint8_t     sweep_divider;
    uint8_t     sweep_divider_period;
    uint8_t     sweep_shift_count;
    uint16_t    sweep_target_period;
    uint8_t     env_divider;
    uint8_t     env_counter;
    uint8_t     volume;
    uint16_t    output;
} nes_apu_pulse;

typedef struct nes_apu
{
    uint8_t     reg_rw_mode : 2;
    uint16_t    reg_addr;
    uint8_t     reg_data;

    uint8_t     sequencer_mode : 1;
    uint8_t     sequencer_reset_req : 1;
    uint8_t     inhibit_interrupt : 1;
    uint8_t     frame_interrupt : 1;

    union
    {
        struct
        {
            uint8_t pulse1   : 1;
            uint8_t pulse2   : 1;
            uint8_t triangle : 1;
            uint8_t noise    : 1;
            uint8_t dmc      : 1;
        };
        uint8_t b;
    } channel_enable;

    nes_apu_pulse pulse[2];

    int16_t     samples[44100];
    uint32_t    sample_count;
    uint32_t    sample_cycle;

    int         odd_cycle;
    uint32_t    cycle;
} nes_apu;

void nes_apu_reset(nes_apu* apu)
{
    memset(apu, 0, sizeof(nes_apu));
}

void update_sweep_target_period(nes_apu* apu, int i)
{
    const uint16_t change_amount = apu->pulse[i].timer >> apu->pulse[i].sweep_shift_count;
    apu->pulse[i].sweep_target_period = apu->pulse[i].timer + (change_amount ^ (0xFFFF * apu->pulse[i].sweep_negate)) + apu->pulse[i].sweep_negate * i;
}

void nes_apu_execute(nes_apu* apu)
{
    const uint8_t lengths[] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30 };

    if (apu->reg_rw_mode)
    {
        switch (apu->reg_addr)
        {
            case NES_APU_STATUS_REG_ID:
            {
                if (apu->reg_rw_mode == NES_APU_REG_RW_MODE_WRITE)
                {
                    apu->channel_enable.b = apu->reg_data & 0x1F;
                    if (apu->channel_enable.pulse1 == 0)
                        apu->pulse[0].length_counter = 0;

                    if (apu->channel_enable.pulse2 == 0)
                        apu->pulse[1].length_counter = 0;
                }
            }
            break;
            case NES_APU_FRAME_COUNTER_REG_ID:
            {
                apu->sequencer_mode = apu->reg_data >> 7;
                apu->inhibit_interrupt = (apu->reg_data >> 6) & 1;
                if (apu->inhibit_interrupt)
                    apu->frame_interrupt = 0;
                apu->sequencer_reset_req = 1;
            }
            break;
            case NES_APU_PULSE1_REG0_ID:
            case NES_APU_PULSE2_REG0_ID:
            {
                unsigned i = (apu->reg_addr >> 2) & 1;
                apu->pulse[i].duty = apu->reg_data >> 6;
                apu->pulse[i].length_counter_halt = (apu->reg_data >> 5) & 1; // alias env_loop
                apu->pulse[i].constant_volume = (apu->reg_data >> 4) & 1;
                apu->pulse[i].volume = apu->reg_data & 0x0F;
            }
            break;
            case NES_APU_PULSE1_REG1_ID:
            case NES_APU_PULSE2_REG1_ID:
            {
                unsigned i = (apu->reg_addr >> 2) & 1;
                apu->pulse[i].sweep_enable = (apu->reg_data >> 7);
                apu->pulse[i].sweep_divider_period = (apu->reg_data >> 4) & 7;
                apu->pulse[i].sweep_negate = (apu->reg_data >> 3) & 1;
                apu->pulse[i].sweep_shift_count = apu->reg_data & 7;
                apu->pulse[i].sweep_reload = 1;
                update_sweep_target_period(apu, i);
            }
            break;
            case NES_APU_PULSE1_REG2_ID:
            case NES_APU_PULSE2_REG2_ID:
            {
                unsigned i = (apu->reg_addr >> 2) & 1;
                apu->pulse[i].timer = (apu->pulse[i].timer & 0xFF00) | apu->reg_data;
                update_sweep_target_period(apu, i);
            }
            break;
            case NES_APU_PULSE1_REG3_ID:
            case NES_APU_PULSE2_REG3_ID:
            {
                unsigned i = (apu->reg_addr >> 2) & 1;
                apu->pulse[i].timer = (((uint16_t)apu->reg_data & 7) << 8) | (apu->pulse[i].timer & 0xFF);
                apu->pulse[i].length_counter = lengths[apu->reg_data >> 3];
                apu->pulse[i].sequencer = 0;
                apu->pulse[i].env_start = 1;
                update_sweep_target_period(apu, i);
            }
            break;
        }
        apu->reg_rw_mode = NES_APU_REG_RW_MODE_NONE;
    }

    // Update triangle channel

    if (apu->odd_cycle == 1)
    {
        unsigned update_quarter = 0;
        unsigned update_half = 0;

        if (apu->sequencer_reset_req)
        {
            apu->sequencer_reset_req = 0;
            apu->cycle = 0;
            update_half = 1;
            update_quarter = 1;
        }

        // Update frame sequencer (odd) on n + 0.5 cycles
        if (apu->sequencer_mode == 0)
        {
            switch(apu->cycle)
            {
                case 3728:  update_quarter = 1; break;
                case 7456:  update_quarter = 1; update_half = 1; break;
                case 11185: update_quarter = 1; break;
                case 14914: update_quarter = 1; update_half = 1; apu->frame_interrupt = (apu->inhibit_interrupt == 0); break;
            }
        }
        else
        {
            switch(apu->cycle)
            {
                case 3728:  update_quarter = 1; break;
                case 7456:  update_quarter = 1; update_half = 1; break;
                case 11185: update_quarter = 1; break;
                case 18640: update_quarter = 1; update_half = 1; break;
            }
        }

        // Clock envelopes & triangle's linear counter
        if (update_quarter)
        {
        }

        // Update pulse channels
        for (int i = 0; i < 2; ++i)
        {
            if ((apu->channel_enable.b & (i + 1)) && apu->pulse[i].length_counter && apu->pulse[i].timer >= 8)
            {
                // Update envelopes
                if (update_quarter)
                {
                    if (apu->pulse[i].env_start)
                    {
                        apu->pulse[i].env_start = 0;
                        apu->pulse[i].env_counter = 15;
                        apu->pulse[i].env_divider = apu->pulse[i].volume + 1;
                    }
                    else if (apu->pulse[i].env_divider-- == 0)
                    {
                        apu->pulse[i].env_divider = apu->pulse[i].volume;
                        if (apu->pulse[i].env_counter-- == 0)
                            apu->pulse[i].env_counter = 15 * apu->pulse[i].env_loop;
                    }
                }

                // Update length counter and sweep unit
                if (update_half)
                {
                    apu->pulse[i].length_counter -= apu->pulse[i].length_counter_halt ^ 1;

                    if (apu->pulse[i].sweep_divider == 0 && apu->pulse[i].sweep_enable && apu->pulse[i].sweep_target_period < 0x800)
                    {
                        apu->pulse[i].timer = apu->pulse[i].sweep_target_period;
                        update_sweep_target_period(apu, i);
                    }

                    if (apu->pulse[i].sweep_divider-- == 0 || apu->pulse[i].sweep_reload)
                    {
                        apu->pulse[i].sweep_divider = apu->pulse[i].sweep_divider_period;
                        apu->pulse[i].sweep_reload = 0;
                    }
                }

                if (apu->pulse[i].t-- == 0)
                {
                    apu->pulse[i].t = apu->pulse[i].timer;
                    switch (apu->pulse[i].duty)
                    {
                        case 0: apu->pulse[i].sequence_output = (0x80 >> apu->pulse[i].sequencer) & 1; break;
                        case 1: apu->pulse[i].sequence_output = (0xC0 >> apu->pulse[i].sequencer) & 1; break;
                        case 2: apu->pulse[i].sequence_output = (0xF0 >> apu->pulse[i].sequencer) & 1; break;
                        case 3: apu->pulse[i].sequence_output = ((~0xC0) >> apu->pulse[i].sequencer) & 1; break;
                    }
                    apu->pulse[i].sequencer = (apu->pulse[i].sequencer - 1) & 7;
                }

                if (apu->pulse[i].constant_volume)
                    apu->pulse[i].output = (apu->pulse[i].sequence_output & (apu->pulse[i].sweep_target_period < 0x800)) * apu->pulse[i].volume;
                else
                    apu->pulse[i].output = (apu->pulse[i].sequence_output & (apu->pulse[i].sweep_target_period < 0x800)) * apu->pulse[i].env_counter;
            }
            else
            {
                apu->pulse[i].output= 0;
            }
        }

        apu->cycle++;
    }
    else
    {
        // Update frame sequencer (even)
        if (apu->sequencer_mode == 0)
        {
            switch(apu->cycle)
            {
                case 14915: apu->cycle = 0;
                case 14914: apu->frame_interrupt = (apu->frame_interrupt == 0); break;
                default: break;
            }
        }
        else if (apu->cycle == 18641)
        {
            apu->cycle = 0;
        }
    }
    apu->odd_cycle ^= 1;
    
    // dummy mixing
//    if (apu->sample_cycle >= 3125)
    {
 //       apu->sample_cycle %= 3125;
        apu->samples[apu->sample_count++] = (apu->pulse[0].output + apu->pulse[1].output) * 1000 - 500;
    }
    apu->sample_cycle += 77;
}

#endif
