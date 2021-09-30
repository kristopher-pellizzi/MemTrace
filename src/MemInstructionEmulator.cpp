#include "MemInstructionEmulator.h"

uint8_t* MemInstructionEmulator::cutUselessBits(uint8_t* uninitializedInterval, ADDRINT addr, UINT32 byteSize){
    unsigned offset = addr % 8;
    UINT32 size = byteSize + offset;
    UINT32 shadowSize = size % 8 != 0 ? (size / 8) + 1 : size / 8;
    UINT32 retSize = byteSize % 8 != 0 ? (byteSize / 8) + 1 : byteSize / 8;
    uint8_t* ret = (uint8_t*) calloc(retSize, sizeof(uint8_t));

    uint8_t* srcPtr = uninitializedInterval + shadowSize - 1;
    uint8_t* dstPtr = ret + retSize - 1;
    
    for(UINT32 i = 0; i < retSize; ++i){
        *dstPtr = *srcPtr >> offset;
        if(offset == 0){
            --dstPtr;
            --srcPtr;
            continue;
        }
        --srcPtr;
        *dstPtr |= (*srcPtr << (8 - offset));
        --dstPtr;
    }
    return ret;
}