#pragma once

#include "lame/lame.h"

class LameWrapper
{
public:
    LameWrapper(lame_t lame);
    ~LameWrapper();
    lame_t get();
private:
    lame_t lame;
};