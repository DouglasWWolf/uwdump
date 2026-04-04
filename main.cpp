//=================================================================================================
// uwdump - Creates a human readable report of userwave events
//
// Author: D. Wolf
//
// Ver    Date      Who  What
//---------------------------------------------------------------------------------------------
// 1.0   31-Mar-26  DWW  Initial creation
//=================================================================================================
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cassert>
#include <vector>
#include "PhysMem.h"
#include "structs.h"
#include "uw_engine.h"
using std::string;
using std::vector;

PhysMem memory;

enum task_t
{
    TASK_UNKNOWN,
    TASK_DUMP_FROM_RAM,
    TASK_DUMP_FROM_FILE,
    TASK_DUMP_FROM_UW,
    TASK_SAVE_TO_FILE
};

// These are command-line options
struct t_opt
{
    task_t      task = TASK_UNKNOWN;
    string      filename;
    uint64_t    base_addr;
} opt;

FILE* ifile;
UWEngine uw_engine(ifile);

uint16_t current_flags;
dacs_t   current_dac;

uw_state_t eof_state;
uint32_t next_index;

const char* bit_name[] = 
{
    "eof",
    "liq_sw",
    "vpretop_sw", "vprebot_sw",
    "refp_sw", "refn_sw",
    "glb_pre_sw",
    "rs0", "rs256",
    "pre0", "pre256"
};


const char* dac_name[] =
{
    "vpretop_a", "vpretop_b", "liq_a",     "liq_b",  nullptr,  nullptr, nullptr,  nullptr,
    nullptr,     "vprebot_a", "vprebot_b", "refp_a", "refp_b", nullptr, "refn_a", "refn_b"
};


//=============================================================================
// Forward declarations
//=============================================================================
void execute(int argc, const char** argv);
//=============================================================================

//=============================================================================
// main() - We just call execute and catch the exceptions
//=============================================================================
int main(int argc, const char** argv)
{
    try
    {
        execute(argc, argv);
    }
    
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        exit(1);
    }

    return 0;
}
//=============================================================================


//=================================================================================================
// throw_runtime() - Throws a runtime exception
//=================================================================================================
static void throw_runtime(const char* fmt, ...)
{
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    throw std::runtime_error(buffer);
}
//=================================================================================================


//=================================================================================================
// str_to_u64() - Convert a string to a uint64_t
//=================================================================================================
uint64_t str_to_u64(string& s)
{
    char token[1000], *out = token;

    // Get a handy pointer to the string
    const char* p = s.c_str();

    // Strip out the underscores
    while (*p)
    {   
        if (*p != '_') *out++ = *p;
        ++p;
    }

    // Return the value of the string
    return strtoull(token, nullptr, 0);
}
//=================================================================================================



//=================================================================================================
// show_help() - Display help text for the user
//=================================================================================================
void show_help()
{
    printf("uwdump <address>\n");
    printf(" -or-\n");
    printf("uwdump <filename>\n");
    printf(" -or-\n");    
    printf("uwdump <address> -save <filename>\n");
    printf(" -or-\n");    
    printf("uwdump -uw <userwave_bin_filename>\n");
    exit(1);    
}
//=================================================================================================


//=================================================================================================
// parse_command_line() - Fills in the "opt" structure from the command line options
//=================================================================================================
void parse_command_line(const char** argv)
{
    ++argv;

    string param;

    while (*argv)
    {
        // Fetch the option
        string option = *argv++;

        // Ignore empty tokens
        if (option.empty()) continue;

        if (option == "-save" && *argv)
        {
            opt.task = TASK_SAVE_TO_FILE;
            opt.filename = *argv++;
            continue;
        }

        if (option == "-uw" && *argv)
        {
            opt.task = TASK_DUMP_FROM_UW;
            opt.filename = *argv++;
            continue;
        }

        // If we don't recognize the command line option, complain
        if (option[0] == '-') 
        {
            throw_runtime("Unknown command line option '%s'", option.c_str());
        }

        // If we don't have base address yet, store it
        if (param.empty())
        {
            param = option;
            continue;
        }

        // If we get here, the user gave us too many filenames
        show_help();
    }

    // Convert the parameter to a number
    opt.base_addr = str_to_u64(param);

    // Sanity check
    if (opt.task == TASK_SAVE_TO_FILE)
    {
        if (opt.base_addr == 0)    
            show_help();
        else
            return;
    }

    // If we're dumping from a userwave, we have all the info we need
    if (opt.task == TASK_DUMP_FROM_UW) return;

    // If the parameter is a base address, we're dumping from RAM
    if (opt.base_addr)
        opt.task = TASK_DUMP_FROM_RAM;
    else
    {
        if (param.empty()) show_help();
        opt.task = TASK_DUMP_FROM_FILE;
        opt.filename = param;
    }

}
//=================================================================================================




//=================================================================================================
// get_tick() - Fetch the uw_state at a specified index
//=================================================================================================
uw_state_t get_tick(uint64_t index)
{
    uw_state_t state;

    // If we're dumping the output state of a userwave, we should never
    // get here
    assert (opt.task != TASK_DUMP_FROM_UW);

    // If we're reading our ticks from a file...
    if (opt.task == TASK_DUMP_FROM_FILE)
    {
        if (fseek(ifile, index * 64, SEEK_SET) != 0)
            return eof_state;
        if (fread(&state, 1, sizeof state, ifile) != sizeof state)
            return eof_state;
        return state;
    }

    // One megabyte
    const  uint64_t window_size  = 0x100000;
    static uint64_t window_start = 0;
    static uint64_t window_end   = 0;
    static uint8_t* window;
    
    // What's the RAM address of the uw_state we're looking for?
    uint64_t address = opt.base_addr + (index * sizeof(uw_state_t));

    // If the tick we're looking for isn't in the current memory window, 
    // map the correct region of memory
    if (address < window_start || address > window_end)
    {
        window_start = address & ~(window_size-1);
        window_end   = window_start + window_size - 1;
        memory.map(window_start, window_size);
        window = memory.bptr();
    }

    // Compute the offset (in the memory window) of the uw_state we want
    uint64_t index_offset = address - window_start;

    // Hand the resulting uw_state to the caller
    return *(uw_state_t*) (window + index_offset);
}
//=================================================================================================

//=================================================================================================
// get_first_four_ticks() - Returns a miniature UWC containing the first four ticks of the
//                          userwave.   
//=================================================================================================
uwc_t get_first_four_ticks()
{
    uwc_t uwc;

    if (opt.task == TASK_DUMP_FROM_UW)
    {
        printf("write get first four ticks for get from uw\n");
        exit(1);
    }

    for (uint64_t i=0; i<3; ++i)
    {
        uwc.push_back(get_tick(i));
    }

    return uwc;
}
//=================================================================================================


//=================================================================================================
// get_next_uwc() - Fetches an entire userwave command
//=================================================================================================
bool get_next_uwc(uwc_t& v)
{
    // Keep track of which index we're on
    static uint64_t index = 0;

    // Clear the result vector
    v.clear();

    // If we're playing back from a userwave binary, let the
    // engine take care of fetching the UWC
    if (opt.task == TASK_DUMP_FROM_UW)
    {
        return uw_engine.get_next_uwc(v);
    }
    
    while (true)
    {
        // Fetch this tick
        auto state = get_tick(index);
    
        // If we're at the end of the state-list, this is either the end
        // of our recorded data, or it's the last UWC of recorded data
        if (state.flags & MASK_EOF)
        {
            if (v.size() != 0)
                break;
            else
                return false;
        }
    
        // If we've hit the start if the next userwave command, we're done
        if (!v.empty() && state.tick_count == 0) break;

        // Add this state to our vector
        v.push_back(state);

        // Setup the index for the next time through the loop
        ++index;

    }

    // Tell the caller that we're handing them a userwave command
    return true;
}
//=================================================================================================


//=================================================================================================
// display_dacs() - Prints the values of the dacs
//=================================================================================================
void display_dacs(uw_state_t& state)
{
    // Display the values of the DACs
    uint16_t* curr = (uint16_t*)&current_dac;
    uint16_t* dacs = (uint16_t*)&state.dac;
    for (int i=0; i<16; i++)
    {
        if (dac_name[i]) 
        {
            if (state.timestamp == 0 || dacs[i] != curr[i])
            {
                printf("%13s = 0x%04X @ %d\n", dac_name[i], dacs[i], state.tick_count);
                curr[i] = dacs[i];
            }
        }
    }
}
//=================================================================================================


//=================================================================================================
// display_signal() - Display information about state changes in a signal that may change more
//                    than once in the course of a UWC
//=================================================================================================
void display_signal(uwc_t& uwc, const char* name, uint16_t mask)
{
    uint16_t last_tick = 0;
    uint16_t start_time = 0;
    uint16_t end_time = 0;
    uint16_t duration = 0;

    struct change_t
    {
        uint16_t    tick_count;
        bool        value;
    } change;

    vector<change_t> v;

    uint16_t seek_state = mask;

    // Loop through each state in UWC
    for (auto& state : uwc)
    {
        // Fetch the value of this bit
        uint16_t state_bit = state.flags & mask;
        
        // If we found the state we're looking for...
        if (state_bit == seek_state)
        {
            if (seek_state)
                change.tick_count = state.tick_count;
            else
                change.tick_count = state.tick_count - 1;
            change.value      = (state_bit != 0);
            v.push_back(change);
            seek_state = (seek_state == mask) ? 0 : mask;
        }

        // Keep track of the last tick number in the UWC
        last_tick = state.tick_count;
    }

    // If we never saw the signal fall, assume
    // that it falls right after the UWC ends
    if (seek_state == 0)
    {
        change.tick_count = last_tick;
        change.value      = false;
        v.push_back(change);
    }
    

    // Loop through each change and display the duration of the signal
    for (auto& ch : v)
    {
        if (ch.value)
            start_time = ch.tick_count;
        else
        {
            end_time = ch.tick_count;
            duration = end_time - start_time + 1;
            printf("%13s = 1 @ %u : %u\n", name, start_time, duration);
        }
    }
}
//=================================================================================================


//=================================================================================================
// display_vswitch() - Give information about the change of state of a voltage switch
//=================================================================================================
void display_vswitch(uwc_t& uwc, const char* name, uint16_t mask)
{
    // Turn the current flags into a uint16_t
    uint16_t curr_bit = current_flags & mask;

    // Loop through each state in the UWC
    for (auto& state : uwc)
    {
        // Fetch the value of this bit
        uint16_t state_bit = state.flags & mask;

        // If this state's flag differs from the most recent value
        // for that flag, display it
        if (state_bit != curr_bit)
        {
            printf("%13s = %i @ %d\n", name, (state_bit != 0), state.tick_count);
            curr_bit = state_bit;
            current_flags &= ~mask;
            current_flags |= state_bit;
        }

    }
}
//=================================================================================================


//=================================================================================================
// display_rs() - Show the places where rs0 or rs256 are active
//=================================================================================================
void display_rs(uwc_t& uwc)
{
    // Loop through each state
    for (auto& state : uwc)
    {
        // If rs0 is on...
        if (state.flags & MASK_RS0)
            printf("%13s = 1 @ %d\n", "rs0", state.tick_count);

        // If rs256 is on...
        if (state.flags & MASK_RS256)
            printf("%13s = 1 @ %d\n", "rs256", state.tick_count);

    }    
}
//=================================================================================================



//=================================================================================================
// display_uwc() - Display the changed settings in a uwc
//=================================================================================================
void display_uwc(uwc_t& uwc)
{
    // Find the index of the last tick of our UWC
    int last = uwc.size() - 1;

    // Get the first tick of the UWC
    auto& first = uwc[0];

    // What's the duration of this UWC?
    uint32_t duration = uwc[last].timestamp - first.timestamp + 1;

    // Tell the user about our UWC
    printf("uwc @ %d : %d\n", first.timestamp, duration);

    // Display the DAC values
    display_dacs(first);
    
    // Display the state changes of the voltage switches
    display_vswitch(uwc, "liq_sw",     MASK_LIQ_SW    );
    display_vswitch(uwc, "vpretop_sw", MASK_VPRETOP_SW);
    display_vswitch(uwc, "vprebot_sw", MASK_VPREBOT_SW);
    display_vswitch(uwc, "refp_sw",    MASK_REFP_SW   );
    display_vswitch(uwc, "refn_sw",    MASK_REFN_SW   );

    display_signal(uwc, "glb_pre_sw", MASK_GLB_PRE_SW);
    display_signal(uwc, "pre0",       MASK_PRE0      );
    display_signal(uwc, "pre256",     MASK_PRE256    );

    // Display rs0 and rs256
    display_rs(uwc);

    printf("\n");

}
//=================================================================================================


//=================================================================================================
// sanity_check() - We examine the first four ticks of the userwave recording to determine
//                  whether or not this is probably a userwave recording
//=================================================================================================
void sanity_check()
{
    auto uwc = get_first_four_ticks();

    for (uint64_t index=0; index<4; ++index)
    {
        auto state = get_tick(index);
        if (state.timestamp != index || state.tick_count != index)
        {
            throw_runtime("source is not a userwave recording");
        }
    }

    // If we have an input file open, rewind it
    if (opt.task == TASK_DUMP_FROM_UW) uw_engine.init();
    if (ifile) rewind(ifile);
}
//=================================================================================================



//=================================================================================================
// save_ram_buffer() - Save the buffer to a file
//=================================================================================================
void save_ram_buffer(string filename)
{
    uint64_t index;
    uw_state_t state;

    // Make sure that our input is a userwave recording
    sanity_check();

    const char* fn = filename.c_str();
    FILE* ofile = fopen(fn, "w");
    if (ofile == nullptr) throw_runtime("Can't open %s", fn);

    // We have valid data... write it to the file!
    for (index = 0; index < 0x100000000; ++index)
    {
        state = get_tick(index);
        fwrite(&state, sizeof(state), 1, ofile);
        if (state.flags & MASK_EOF) break;
    }

    fclose(ofile);
}
//=================================================================================================


//=================================================================================================
// dump_userwave_recording() - Parses the userwave recording and prints human-readable output
//=================================================================================================
void dump_userwave_recording()
{
    uwc_t uwc;

    while (get_next_uwc(uwc))
    {
        display_uwc(uwc);
    }
}
//=================================================================================================


//=================================================================================================
// open_input_file() - Opens the input file (duh)
//=================================================================================================
void open_input_file(string filename)
{
    // Get a handy pointer to the filename
    const char* fn = filename.c_str();

    // Open the file and complain if we can't
    ifile = fopen(fn, "r");
    if (ifile == nullptr) throw_runtime("cant open %s", fn);
}
//=================================================================================================



//=================================================================================================
// execute() - Very simple main-line program execution
//=================================================================================================
void execute(int argc, const char** argv)
{

    // Create state record to serve an an EOF
    memset(&eof_state, 0xFF, sizeof eof_state);

    // Parse the command line options
    parse_command_line(argv);

    switch (opt.task)
    {
        case TASK_SAVE_TO_FILE:
            save_ram_buffer(opt.filename);
            break;

        case TASK_DUMP_FROM_RAM:
            dump_userwave_recording();
            break;
        
        case TASK_DUMP_FROM_FILE:
            open_input_file(opt.filename);
            dump_userwave_recording();
            break;
        
        case TASK_DUMP_FROM_UW:
            open_input_file(opt.filename);
            dump_userwave_recording();
            break;

        default:
            fprintf(stderr, "How did we get here?\n");
            exit(1);
    }

    exit(0);
}
//=================================================================================================
