#include "ShadowMemory.h"

static std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr){
    unsigned retOffset = (unsigned) (threadInfos[0] - addr);
    unsigned offset = retOffset;
    offset >>= 3;
    unsigned shadowIdx = offset / SHADOW_ALLOCATION;
    return std::pair<unsigned, unsigned>(shadowIdx, retOffset);
}

static uint8_t* getShadowAddrFromIdx(unsigned shadowIdx, unsigned offset){
    bool needsCeiling = offset % 8 != 0;
    
    offset >>=3;
    // If the requested address does not have a shadow location yet, allocate it
    while(shadowIdx >= shadow.size()){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) - 1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        shadow.push_back(newMap);
    }
    
    uint8_t* ret = shadow[shadowIdx] + (offset % SHADOW_ALLOCATION);
    return needsCeiling ? ret + 1 : ret;
}

static unsigned long min(unsigned long x, unsigned long y){
    return x <= y ? x : y;
}

uint8_t* getShadowAddr(ADDRINT addr){
    std::pair<unsigned, unsigned> shadowIdxOffset = getShadowAddrIdxOffset(addr);

    uint8_t* ret = getShadowAddrFromIdx(shadowIdxOffset.first, shadowIdxOffset.second);
    return ret;
}

void set_as_initialized(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    if(shadowAddr > highestShadowAddr)
        highestShadowAddr = shadowAddr;

    // The address associated to shadowAddr is an 8-bytes aligned address.
    // However, addr may not be 8-bytes aligned. Consider the 8-bytes aligned address
    // represented by shadowAddr, and adjust operations with the offset of the first considered byte
    // from the beginning of that address.
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    while(leftSize >= 8){
        *shadowAddr |= 0xff << offset;
        offset = 0;

        if(shadowAddr != shadow[shadowIdx]){
            --shadowAddr;
        } 
        else if(shadowIdx != 0){
            shadowAddr = shadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
        }
        else{
            leftSize = 0;
            break;
        }

        leftSize -= 8;
    }
    if(leftSize > 0){
        uint8_t val = (1 << (leftSize - offset)) - 1;
        val <<= offset;
        *shadowAddr |= val;
    }
}

std::pair<int, int> getUninitializedInterval(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    bool isUninitialized = false;
    uint8_t* initialShadowAddr = shadowAddr;
    unsigned offset = addr % 8;
    unsigned initialOffset = offset;
    UINT32 leftSize = size + offset;
    uint8_t val;

    while(leftSize >= 8 && !isUninitialized){
        val = *shadowAddr;
        val |= ((1 << offset) - 1);
        if(val != 0xff){
            isUninitialized = true;
        }
        offset = 0;

        if(shadowAddr != shadow[shadowIdx]){
            --shadowAddr;
        }
        else if(shadowIdx != 0){
            shadowAddr = shadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
        }
        else{
            leftSize = 0;
            break;
        }

        leftSize -= 8;
    }
    if(!isUninitialized && leftSize > 0){
        val = *shadowAddr;
        // Change val to put 1 to every bit not considered by this access
        val |= ((1 << offset) - 1);
        val |= (0xff << leftSize);
        if(val != 0xff){
            isUninitialized = true;
        }
    }
    else{
        // While loop exited after having decresed it. In order to compute the uninitialized interval,
        // we need to restore the last address that we checked
        ++shadowAddr;
    }

    if(isUninitialized){
        val >>= initialOffset;
        offset = 0;
        while(val % 2 != 0){
            ++offset;
            val >>= 1;
        }
        std::pair<int, int> ret((initialShadowAddr - shadowAddr) * 8 + offset, size - 1);
        return ret;
    }
    else
        return std::pair<int, int>(-1, -1);
}

void reset(ADDRINT addr){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    unsigned long bottom = min((unsigned long) highestShadowAddr, (unsigned long) (shadowAddr + SHADOW_ALLOCATION - 1));
    memset(shadowAddr, 0, bottom - (unsigned long) shadowAddr + 1);

    for(unsigned i = shadowIdx + 1; i < shadow.size(); ++i){
        if(shadow[i] <= highestShadowAddr && shadow[i] > shadowAddr){
            memset(shadow[i], 0, SHADOW_ALLOCATION);
        }
    }
}