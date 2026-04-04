#pragma once
#include <cstdint>
#include <vector>
using std::vector;

#pragma pack(push, 1)

//=============================================================================
// This is a binary userwave-command, with all fields in little-endian.
//=============================================================================
struct uw_cmd_t
{
    uint32_t    cmd_index;
    uint16_t    cmd_duration;

    uint8_t     read_en                 : 1;
    uint8_t     read_phase              : 1;
    uint8_t     read_bright_flag        : 1;
    uint8_t     read_safe_halting_point : 1;
    uint8_t     read_generated_flag     : 1;
    uint16_t    read_flags_reserved     : 11;

    uint16_t    read_start_time;
    uint16_t    read_characterization_id;
    uint16_t    read_data_type;
    uint16_t    reserved0;
    uint32_t    reserved1;
    
    uint16_t    vpretop_a;
    uint16_t    vpretop_b;
    uint16_t    vpretop_sw_delay;
    
    uint16_t    vprebot_a;
    uint16_t    vprebot_b;
    uint16_t    vprebot_sw_delay;

    uint16_t    refp_a;
    uint16_t    refp_b;
    uint16_t    refp_sw_delay;

    uint16_t    refn_a;
    uint16_t    refn_b;
    uint16_t    refn_sw_delay;

    uint16_t    liq_a;
    uint16_t    liq_b;
    uint16_t    liq_sw_delay;

    uint8_t     vpretop_sw      : 1;
    uint8_t     vprebot_sw      : 1;
    uint8_t     refp_sw         : 1;
    uint8_t     refn_sw         : 1;
    uint8_t     liq_sw          : 1;
    uint16_t    switch_reserved : 11;

    uint16_t    roll_pre_top_duration;
    uint16_t    roll_pre_top_start;

    uint16_t    roll_pre_bot_duration;
    uint16_t    roll_pre_bot_start;

    uint16_t    glb_pre_duration;
    uint16_t    glb_pre_start;
};
//=============================================================================

// Masks for each bit in uw_flags_t
const int MASK_EOF        = 0x0001;
const int MASK_LIQ_SW     = 0x0002;
const int MASK_VPRETOP_SW = 0x0004;
const int MASK_VPREBOT_SW = 0x0008;
const int MASK_REFP_SW    = 0x0010;
const int MASK_REFN_SW    = 0x0020;
const int MASK_GLB_PRE_SW = 0x0040;
const int MASK_RS0        = 0x0080;
const int MASK_RS256      = 0x0100;
const int MASK_PRE0       = 0x0200;
const int MASK_PRE256     = 0x0400;

// A mask for the DAC voltage switches
const int MASK_DAC_SWITCHES = MASK_LIQ_SW
                            | MASK_VPRETOP_SW
                            | MASK_VPREBOT_SW
                            | MASK_REFP_SW
                            | MASK_REFN_SW;

struct dacs_t           
{
    uint16_t    vpretop_a;
    uint16_t    vpretop_b;
    uint16_t    liq_a;
    uint16_t    liq_b;
    uint16_t    na0;
    uint16_t    na1;
    uint16_t    na2;
    uint16_t    na3;        

    uint16_t    na4;
    uint16_t    vprebot_a;
    uint16_t    vprebot_b;
    uint16_t    refp_a;
    uint16_t    refp_b;
    uint16_t    na5;
    uint16_t    refn_a;
    uint16_t    refn_b;
};


struct uw_state_t
{
    uint32_t    timestamp;
    uint16_t    tick_count;
    dacs_t      dac;
    uint16_t    flags;
    uint8_t     filler[24];
};

#pragma pack(pop)


typedef vector<uw_state_t> uwc_t;
