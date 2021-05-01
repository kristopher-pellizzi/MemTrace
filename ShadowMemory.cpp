#include <vector>

#include "ShadowMemory.h"

using std::vector;

static unsigned long SHADOW_ALLOCATION = sysconf(_SC_PAGESIZE);

unsigned long long ShadowBase::min(unsigned long long x, unsigned long long y){
    return x <= y ? x : y;
}

uint8_t* ShadowBase::shadow_memory_copy(ADDRINT addr, UINT32 size){
    ADDRINT lastByteAddr = addr + size - 1;
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(lastByteAddr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    unsigned offset = addr % 8;
    size += offset;
    UINT32 shadowSize = size % 8 != 0 ? (size / 8) + 1 : (size / 8);

    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);
    UINT32 copied = 0;
    while(copied != shadowSize){
        uint8_t* highestCopied = (uint8_t*)min(
            (unsigned long long) shadowAddr + (shadowSize - copied) - 1,
            (unsigned long long) shadow[shadowIdx] + SHADOW_ALLOCATION - 1
        );

        UINT32 toCopy = highestCopied - shadowAddr + 1;
        memcpy(ret + copied, shadowAddr, toCopy);
        copied += toCopy;
        shadowAddr = shadow[++shadowIdx];
    }

    return ret;
}

std::pair<unsigned, unsigned> StackShadow::getShadowAddrIdxOffset(ADDRINT addr) {
    unsigned retOffset = (unsigned) (baseAddr - addr);
    unsigned offset = retOffset;
    offset >>= 3;
    unsigned shadowIdx = offset / SHADOW_ALLOCATION;
    return std::pair<unsigned, unsigned>(shadowIdx, retOffset);
}

uint8_t* ShadowBase::getShadowAddrFromIdx(unsigned shadowIdx, unsigned offset){
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

uint8_t* ShadowBase::getShadowAddr(ADDRINT addr){
    std::pair<unsigned, unsigned> shadowIdxOffset = this->getShadowAddrIdxOffset(addr);

    uint8_t* ret = this->getShadowAddrFromIdx(shadowIdxOffset.first, shadowIdxOffset.second);
    return ret;
}

uint8_t* ShadowBase::getReadShadowFromShadow(uint8_t* shadowAddr, unsigned shadowIdx){
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

void StackShadow::set_as_initialized(ADDRINT addr, UINT32 size) {
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

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
            readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);
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

void StackShadow::set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx) {
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

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

uint8_t* StackShadow::getUninitializedInterval(ADDRINT addr, UINT32 size) {
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);

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
        this->set_as_read_by_uninitialized_read(size, initialShadowAddr, initialOffset, shadowIdx);

        return this->shadow_memory_copy(addr, size);
    }
    else
        return NULL;
}

bool StackShadow::isReadByUninitializedRead(ADDRINT addr, UINT32 size) {
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

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

void StackShadow::reset(ADDRINT addr) {
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    unsigned long long bottom = min((unsigned long long) highestShadowAddr, (unsigned long long) (shadowAddr + SHADOW_ALLOCATION - 1));
    memset(shadowAddr, 0, bottom - (unsigned long long) shadowAddr + 1);

    for(unsigned i = shadowIdx + 1; i < shadow.size(); ++i){
        if(dirtyPages[i]){
            memset(shadow[i], 0, SHADOW_ALLOCATION);
            dirtyPages[i] = false;
        }
    }
}

void ShadowBase::setBaseAddr(ADDRINT baseAddr){
    this->baseAddr = baseAddr;
}

ShadowBase* ShadowBase::getPtr(){
    return this;
}

StackShadow::StackShadow(){
    shadow.reserve(5);
    readShadow.reserve(5);
    dirtyPages.reserve(5);

    for(int i = 0; i < 2; ++i){
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

HeapShadow::HeapShadow(){
    shadow.reserve(5);
    readShadow.reserve(5);
    dirtyPages.reserve(5);

    for(int i = 0; i < 2; ++i){
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
}

std::pair<unsigned, unsigned> HeapShadow::getShadowAddrIdxOffset(ADDRINT addr){
    unsigned retOffset = (unsigned) (addr - baseAddr);
    unsigned offset = retOffset;
    offset >>= 3;
    unsigned shadowIdx = offset / SHADOW_ALLOCATION;
    return std::pair<unsigned, unsigned>(shadowIdx, retOffset);
}

void HeapShadow::set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx){
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

    UINT32 leftSize = size + offset;
    while(leftSize >= 8){
        *readShadowAddr |= 0xff >> offset;
        offset = 0;

        if(readShadowAddr != readShadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++readShadowAddr;
        } 
        else{
            readShadowAddr = readShadow[++shadowIdx];
        }

        leftSize -= 8;
    }
    if(leftSize > 0){
        uint8_t mask = ~(0xff >> leftSize);
        mask >>= offset;
        *readShadowAddr |= mask;
    }
}

void HeapShadow::set_as_initialized(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

    dirtyPages[shadowIdx] = true;

    // The address associated to shadowAddr is an 8-bytes aligned address.
    // However, addr may not be 8-bytes aligned. Consider the 8-bytes aligned address
    // represented by shadowAddr, and adjust operations with the offset of the first considered byte
    // from the beginning of that address.
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    while(leftSize >= 8){
        uint8_t mask = (0xff >> offset);
        *shadowAddr |= mask;
        *readShadowAddr &= ~mask;
        offset = 0;

        if(shadowAddr != shadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++shadowAddr;
            ++readShadowAddr;
        } 
        else{
            shadowAddr = shadow[++shadowIdx];
            readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);
            dirtyPages[shadowIdx] = true;
        }

        leftSize -= 8;
    }
    if(leftSize > 0){
        uint8_t mask = ~(0xff >> leftSize);
        mask >>= offset;
        *shadowAddr |= mask;
        *readShadowAddr &= ~mask;
    }

    if(shadowAddr > highestShadowAddr)
        highestShadowAddr = shadowAddr;
}

uint8_t* HeapShadow::getUninitializedInterval(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);

    bool isUninitialized = false;
    uint8_t* initialShadowAddr = shadowAddr;
    unsigned offset = addr % 8;
    unsigned initialOffset = offset;
    UINT32 leftSize = size + offset;
    uint8_t val;

    while(leftSize >= 8 && !isUninitialized){
        val = *shadowAddr;
        val |= ~(0xff >> offset);
        if(val != 0xff){
            isUninitialized = true;
        }
        offset = 0;

        if(shadowAddr != shadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++shadowAddr;
        }
        else{
            shadowAddr = shadow[++shadowIdx];
        }

        leftSize -= 8;
    }
    if(!isUninitialized && leftSize > 0){
        val = *shadowAddr;
        // Change val to put 1 to every bit not considered by this access
        uint8_t mask = ~(0xff >> (leftSize - offset));
        mask = ~(mask >> offset);
        val |= mask;
        if(val != 0xff){
            isUninitialized = true;
        }
    }
    else{
        // While loop exited after having decresed it. In order to compute the uninitialized interval,
        // we need to restore the last address that we checked
        --shadowAddr;
    }

    if(isUninitialized){
        this->set_as_read_by_uninitialized_read(size, initialShadowAddr, initialOffset, shadowIdx);

        return this->shadow_memory_copy(addr, size);
    }
    else
        return NULL;
}

bool HeapShadow::isReadByUninitializedRead(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    uint8_t* readShadowAddr = this->getReadShadowFromShadow(shadowAddr, shadowIdx);

    bool isRead = false;
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    uint8_t val;

    while(leftSize >= 8 && !isRead){
        val = *readShadowAddr;
        val &= 0xff >> offset;
        if(val != 0){
            isRead = true;
        }
        offset = 0;

        if(readShadowAddr != readShadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++readShadowAddr;
        }
        else{
            readShadowAddr = readShadow[++shadowIdx];
        }

        leftSize -= 8;
    }
    if(!isRead && leftSize > 0){
        val = *readShadowAddr;
        val &= 0xff >> offset;
        val &= ~(0xff >> leftSize);
        if(val != 0){
            isRead = true;
        }
    }

    return isRead;
}

void HeapShadow::reset(ADDRINT addr){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(shadowIdx, idxOffset.second);
    size_t freed_size = mallocatedPtrs[addr];
    size_t reset_size = 0;

    while(reset_size < freed_size){
        unsigned long long bottom = min((unsigned long long) shadowAddr + freed_size - reset_size - 1, (unsigned long long) (shadowAddr + SHADOW_ALLOCATION - 1));
        unsigned long long freedBytes = bottom - (unsigned long long) shadowAddr + 1);
        memset(shadowAddr, 0, freedBytes);
        reset_size += freedBytes;
        shadowAddr = shadow[++shadowIdx];
    }
}


StackShadow stack;
HeapShadow heap;

ShadowBase* currentShadow;

uint8_t* getShadowAddr(ADDRINT addr){
    return currentShadow->getShadowAddr(addr);
}

void set_as_initialized(ADDRINT addr, UINT32 size){
    currentShadow->set_as_initialized(addr, size);
}

uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size){
    return currentShadow->getUninitializedInterval(addr, size);
}

bool isReadByUninitializedRead(ADDRINT addr, UINT32 size){
    return currentShadow->isReadByUninitializedRead(addr, size);
}

void reset(ADDRINT addr){
    currentShadow->reset(addr);
}