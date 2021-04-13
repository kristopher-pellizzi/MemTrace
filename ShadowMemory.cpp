#include <vector>

#include "ShadowMemory.h"

using std::vector;

static vector<bool> dirtyPages;
static unsigned long SHADOW_ALLOCATION;

// This shadow memory keeps 1 bit for each byte of application memory,
// telling if that byte has been initialized by a write access
static std::vector<uint8_t*> shadow;

// This second shadow memory keeps 1 additional bit for each byte of application memory,
// telling if that byte has been read by a read access considered uninitialized
static std::vector<uint8_t*> readShadow;

// Returns a pair containing the index to be used to retrieve the correct shadow page
// and an offset required to get the correct address inside that page
static std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr){
    unsigned retOffset = (unsigned) (threadInfos[0] - addr);
    unsigned offset = retOffset;
    offset >>= 3;
    unsigned shadowIdx = offset / SHADOW_ALLOCATION;
    return std::pair<unsigned, unsigned>(shadowIdx, retOffset);
}

// Given the index and the offset returned from the previous function,
// return the corresponding shadow memory address
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
        dirtyPages.push_back(false);
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

// Given a shadow address, return the corresponding shadow address of the read shadow memory
// (the one containing information about bytes read by uninitialized read accesses)
static uint8_t* getReadShadowFromShadow(uint8_t* shadowAddr, unsigned shadowIdx){
    unsigned offset = shadowAddr - shadow[shadowIdx];

    while(shadowIdx >= readShadow.size()){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) - 1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        readShadow.push_back(newMap);
    }

    return readShadow[shadowIdx] + offset;
}

// Function invoked whenever a write access is executed.
// It sets the corresponding shadow memory area to 1 and resets the corresponding
// read shadow memory area to 0
void set_as_initialized(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = getReadShadowFromShadow(shadowAddr, shadowIdx);

    dirtyPages[shadowIdx] = true;

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
        *readShadowAddr &= (1 << offset) - 1;
        offset = 0;

        if(shadowAddr != shadow[shadowIdx]){
            --shadowAddr;
            --readShadowAddr;
        } 
        else if(shadowIdx != 0){
            shadowAddr = shadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
            readShadowAddr = getReadShadowFromShadow(shadowAddr, shadowIdx);
            dirtyPages[shadowIdx] = true;
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

        val = (uint8_t) 0xff << leftSize;
        val |= (1 << offset) - 1;
        *readShadowAddr &= val;
    }
}

// Function invoked whenever an uninitialized read is executed.
// It sets the corresponding read shadow memory area to 1.
static void set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx){
    uint8_t* readShadowAddr = getReadShadowFromShadow(shadowAddr, shadowIdx);

    UINT32 leftSize = size + offset;
    while(leftSize >= 8){
        *readShadowAddr |= 0xff << offset;
        offset = 0;

        if(readShadowAddr != readShadow[shadowIdx]){
            --readShadowAddr;
        } 
        else if(shadowIdx != 0){
            readShadowAddr = readShadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
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
        *readShadowAddr |= val;
    }

}

// Function to retrieve the bytes of the given address and size which are considered not initialized.
// The returned interval contains the offset of the bytes from the beginning of the access.
// For instance, if a write already initialized bytes [0x0, 0xa] of memory and the given address and size access
// [0x0, 0xf], the returned pair would be (11, 15).

// NOTE: this is not a precise result. If the access represented by |addr| and |size| is fully initialized except 1 byte,
// the returned pair will report as uninitialized the whole interval from the uninitialized byte to the end of the
// access.
// This has been done because:
//  [*] It shouldn't be very frequent that an access to an area of the stack has an uninitialized "hole"
//      (this may happen, though, for instance with functions requiring a particular address alignment)
//  [*] It would have been quite expensive to return the precise interval as it would require to always scan all 
//      the bytes of the access and, in case the "hole" was in the middle
//      of the access, it may have required to return a set of intervals, thus making the following steps
//      of the tool more complex.

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
        set_as_read_by_uninitialized_read(size, initialShadowAddr, initialOffset, shadowIdx);

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

// Returns true if the access represented by the given address and size (at least one of its bytes, actually)
// is read by an uninitialized read access
bool isReadByUninitializedRead(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = getReadShadowFromShadow(shadowAddr, shadowIdx);

    bool isRead = false;
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    uint8_t val;

    while(leftSize >= 8 && !isRead){
        val = *readShadowAddr;
        val &= 0xff << offset;
        if(val != 0){
            isRead = true;
        }
        offset = 0;

        if(readShadowAddr != readShadow[shadowIdx]){
            --readShadowAddr;
        }
        else if(shadowIdx != 0){
            readShadowAddr = readShadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
        }
        else{
            leftSize = 0;
            break;
        }

        leftSize -= 8;
    }
    if(!isRead && leftSize > 0){
        val = *readShadowAddr;
        val &= 0xff << offset;
        val &= (1 << leftSize) - 1;
        if(val != 0){
            isRead = true;
        }
    }

    return isRead;
}

// Resets all the shadow memory addresses above or equal to |addr| to 0.
// This is invoked on return instructions, when a stack frame is "freed".
void reset(ADDRINT addr){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    unsigned long bottom = min((unsigned long) highestShadowAddr, (unsigned long) (shadowAddr + SHADOW_ALLOCATION - 1));
    memset(shadowAddr, 0, bottom - (unsigned long) shadowAddr + 1);

    for(unsigned i = shadowIdx + 1; i < shadow.size(); ++i){
        if(dirtyPages[i]){
            memset(shadow[i], 0, SHADOW_ALLOCATION);
            dirtyPages[i] = false;
        }
    }
}

// Shadow memory initialization
void shadowInit(){
    shadow.reserve(25);
    readShadow.reserve(25);
    dirtyPages.reserve(25);
    long pagesize = sysconf(_SC_PAGESIZE);
    SHADOW_ALLOCATION = pagesize;
    for(int i = 0; i < 3; ++i){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) -1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        shadow.push_back(newMap);
        dirtyPages[i] = false;

        newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) -1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        readShadow.push_back(newMap);
    }
    // The address contained by the stack pointer at the beginning of the application entry point is initialized
    // outside the function itself (i.e. by the loader) but the entry point reads it through a pop instruction.
    // In order to avoid to report a false positive, set as initialized the shadow memory mirroring that address.
    *shadow[0] = 0xff;
    dirtyPages[0] = true;
}