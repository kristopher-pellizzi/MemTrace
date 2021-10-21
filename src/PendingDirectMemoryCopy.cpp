#include "PendingDirectMemoryCopy.h"

PendingDirectMemoryCopy::PendingDirectMemoryCopy() :
    ma(MemoryAccess()),
    valid(false)
{}

PendingDirectMemoryCopy::PendingDirectMemoryCopy(MemoryAccess& ma) :
    ma(ma),
    valid(true)
{}

ADDRINT PendingDirectMemoryCopy::getIp(){
    return ma.getActualIP();
}

MemoryAccess& PendingDirectMemoryCopy::getAccess(){
    return ma;
}

bool PendingDirectMemoryCopy::isValid(){
    return valid;
}

void PendingDirectMemoryCopy::setAsInvalid(){
    valid = false;
}