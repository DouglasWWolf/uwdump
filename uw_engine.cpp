#include "uw_engine.h"
#include <cstring>

static uw_state_t eof_state, blank_state;

void UWEngine::init()
{
    timestamp_ = 0;
    fseek(ifile_, 0, SEEK_SET);
    memset(&eof_state, 0xFF, sizeof eof_state);
    memset(&blank_state, 0, sizeof blank_state);
}

//=============================================================================
// set_flag() - Turns on one of the bits in the flags field for one or 
//                more ticks of the UWC
//=============================================================================
static void set_flag(uwc_t& uwc, uint16_t mask, int start, int duration)
{
    // Safety check
    if (uwc.empty()) return;
        
    // What's the last index in the UWC?
    uint16_t last_index = uwc.size() - 1;

    // Loop through each state that we care about
    for (int i=0; i<duration; ++i)
    {
        // What index are we about to update?
        uint16_t index = start + i;        

        // Have we fallen off the end of the UWC?
        if (index > last_index) break;

        uwc[index].flags |= mask;
    }

}
//=============================================================================



//=============================================================================
// get_next_uwc() - Creates a UWC structure from the next userwave command 
//=============================================================================
bool UWEngine::get_next_uwc(uwc_t& uwc)
{
    uw_cmd_t    cmd;
    int         i;
    uw_state_t  state;

    // Start with an empty UWC
    uwc.clear();

    // If there are no more userwave commands, hand the caller an EOF
    if (fread(&cmd, 1, sizeof cmd, ifile_) != sizeof cmd)
    {
        for (i=0; i<4; ++i) uwc.push_back(eof_state);
        return false;
    }

    // Create a UWC with the timestamp, tick_count, and DAC values all filled in
    uint16_t tick = 0;
    memset(&state, 0, sizeof state);
    state.dac.vpretop_a = cmd.vpretop_a;
    state.dac.vpretop_b = cmd.vpretop_b;
    state.dac.vprebot_a = cmd.vprebot_a;
    state.dac.vprebot_b = cmd.vprebot_b;
    state.dac.liq_a     = cmd.liq_a;
    state.dac.liq_b     = cmd.liq_b;
    state.dac.refp_a    = cmd.refp_a;
    state.dac.refp_b    = cmd.refp_b;
    state.dac.refn_a    = cmd.refn_a;
    state.dac.refn_b    = cmd.refn_b;
    for (i=0; i < cmd.cmd_duration; ++i)
    {
        state.timestamp  = timestamp_++;
        state.tick_count = tick++;
        uwc.push_back(state);
    }

    // If a read occurs during this UWC...
    if (cmd.read_en)
    {
        if (cmd.read_phase == 0)
            set_flag(uwc, MASK_RS0,   cmd.read_start_time, 1);
        else
            set_flag(uwc, MASK_RS256, cmd.read_start_time, 1);
    }

    // Turn on the switches at the appropriate time ticks
    set_flag(uwc, MASK_VPRETOP_SW, cmd.vpretop_sw_delay,   cmd.cmd_duration);
    set_flag(uwc, MASK_VPREBOT_SW, cmd.vprebot_sw_delay,   cmd.cmd_duration);
    set_flag(uwc, MASK_REFP_SW,    cmd.refp_sw_delay,      cmd.cmd_duration);
    set_flag(uwc, MASK_REFN_SW,    cmd.refn_sw_delay,      cmd.cmd_duration);
    set_flag(uwc, MASK_LIQ_SW,     cmd.liq_sw_delay,       cmd.cmd_duration);
    set_flag(uwc, MASK_GLB_PRE_SW, cmd.glb_pre_start,      cmd.glb_pre_duration);
    set_flag(uwc, MASK_PRE0,       cmd.roll_pre_top_start, cmd.roll_pre_top_duration);
    set_flag(uwc, MASK_PRE256,     cmd.roll_pre_bot_start, cmd.roll_pre_bot_duration);

    // Tell the caller that we're handing them a valid UWC
    return true;
}
//=============================================================================

