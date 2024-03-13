#pragma once

#include "lame/lame.h"

class LameWrapper
{
public:
    LameWrapper();
    ~LameWrapper();
    lame_t get();
private:
    lame_t lame;
};