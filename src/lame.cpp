#include "lame.h"

LameWrapper::LameWrapper()
{
    lame = lame_init();
}

LameWrapper::~LameWrapper()
{
    lame_close(lame);
}

lame_t LameWrapper::get()
{
    return lame;
}