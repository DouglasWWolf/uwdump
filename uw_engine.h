#pragma once
#include <cstdio>
#include "structs.h"

class UWEngine
{
public:
    UWEngine(FILE*& ifile) : ifile_(ifile) {}
    void    init();
    bool    get_next_uwc(uwc_t& uwc);

protected:

    FILE*&      ifile_;
    uint64_t    timestamp_;
    int16_t     dac_switches_;
};