#include "MemoryStatus.h"

/*
    Cuts the bits of the |uninitializedInterval| which are there because of an existing offset, thus returning
    only the bits related to the uninitialized read itself and those additional bits existing because the size of the
    read is not a multiple of 8 bytes.
    This is used by LOAD instructions.
*/
uint8_t* cutUselessBits(uint8_t* uninitializedInterval, ADDRINT addr, UINT32 byteSize){
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

// Add the offset to the bit word (i.e. shift the whole bitmask by |offset| bits to the left) to prepare it to be stored.
// This is used by STORE instructions.
uint8_t* addOffset(uint8_t* data, unsigned offset, unsigned* srcShadowSize, unsigned srcByteSize){
    uint8_t* ret = data;
    unsigned origShadowSize = *srcShadowSize;

    if(srcByteSize % 8 == 0 || offset + srcByteSize % 8 > 8){
        ++(*srcShadowSize);
        ret = (uint8_t*) malloc(sizeof(uint8_t) * (*srcShadowSize));
    }

    unsigned i = 0;
    unsigned j = 0;
    // If we had to allocate a new byte, we need to store the 8 - offset most significant bytes in the least significant 
    // bytes of the newly created byte
    if(ret != data){
        *ret = (uint8_t) 0 | (*data >> (8 - offset));
        ++i;
    }

    for(; i < *srcShadowSize; ++i){
        *(ret + i) = *(data + j++) << offset;
        if(j < origShadowSize)
            *(ret + i++) |= *(data + j) >> (8 - offset);
    }

    return ret;

}