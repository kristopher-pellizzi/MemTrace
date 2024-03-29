#include <vector>

#include "ShadowMemory.h"

using std::vector;

unordered_map<ADDRINT, HeapShadow> mmapShadows;
unsigned long mmapShadowsCounter = 0;

static unsigned long SHADOW_ALLOCATION = sysconf(_SC_PAGESIZE);

unsigned long long ShadowBase::min(unsigned long long x, unsigned long long y){
    return x <= y ? x : y;
}

uint8_t* ShadowBase::shadow_memory_copy(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);
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

uint8_t* ShadowBase::getShadowAddr(ADDRINT addr){
    std::pair<unsigned, unsigned> shadowIdxOffset = this->getShadowAddrIdxOffset(addr);

    uint8_t* ret = this->getShadowAddrFromIdx(&shadowIdxOffset.first, shadowIdxOffset.second);
    return ret;
}



uint8_t* StackShadow::getShadowAddrFromIdx(unsigned* shadowIdxPtr, unsigned offset){
    unsigned shadowIdx = *shadowIdxPtr;
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

    if(needsCeiling){
        // If |ret| is the address of the last byte of this shadow page, we need to increase shadowIdx in order
        // to "increase" its value, and take the address of the first byte of the next shadow page.
        if(ret == shadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ret = shadow[shadowIdx + 1];
            ++(*shadowIdxPtr);
        }
        // If |ret| is anything but the address of the last byte of the considered shadow page, just increase its value by 1.
        else{
            ++ret;
        }
    }

    return ret;
}

void StackShadow::set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    dirtyPages[shadowIdx] = true;

    if(shadowAddr > highestShadowAddr)
        highestShadowAddr = shadowAddr;

    // The address associated to shadowAddr is an 8-bytes aligned address.
    // However, addr may not be 8-bytes aligned. Consider the 8-bytes aligned address
    // represented by shadowAddr, and adjust operations with the offset of the first considered byte
    // from the beginning of that address.
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    unsigned shadowSize = leftSize % 8 != 0 ? (leftSize / 8 + 1) : leftSize / 8;
    uint8_t* src = data + shadowSize - 1;

    // Note: |data| already contains offset and possible additional bytes.
    // They are all set to 0, so that it is enough to bitwise OR |data| with the shadow memory
    
    for(unsigned i = 0; i < shadowSize; ++i){
        // Reset bits related to our access
        uint8_t mask = 0;
        if(i == 0){
            mask |= (uint8_t) 0xff >> (8 - offset);
        }
        if(leftSize % 8 != 0 && i == shadowSize - 1){
            mask |= (uint8_t) 0xff << (leftSize % 8);
        }
        *shadowAddr &= mask;

        *shadowAddr |= *src;

        if(shadowAddr != shadow[shadowIdx]){
            --shadowAddr;
        } 
        else if(shadowIdx != 0){
            shadowAddr = shadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
            dirtyPages[shadowIdx] = true;
        }
        else{
            leftSize = 0;
            break;
        }

        --src;
    }
}

void StackShadow::set_as_initialized(ADDRINT addr, UINT32 size) {
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

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
        offset = 0;

        if(shadowAddr != shadow[shadowIdx]){
            --shadowAddr;
        } 
        else if(shadowIdx != 0){
            shadowAddr = shadow[--shadowIdx] + SHADOW_ALLOCATION - 1;
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
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    bool isUninitialized = false;
    unsigned offset = addr % 8;
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
        return this->shadow_memory_copy(addr + size - 1, size);
    }
    else
        return NULL;
}

/*
** Note that since StackShadow::reset resets to 0 all the addresses below |addr| (and therefore all the shadow addresses above the shadow address
** corresponding to |addr|), it is not required to consider possible offsets and remaining bytes.
** Indeed, even if there is any offset, the whole corresponding shadow byte is guaranteed to be reset (notice that |addr| is aligned either to 8 or to 4 bytes boundaries
** according to the used architecture).
** E.g. on x86, if |addr| = 0x7fffffff0004, the corresponding shadow byte will contain, besides that 4 bytes word associated to |addr|, also the 4 bytes word associated to
** 0x7fffffff0000, which is lower than |addr|, and therefore must be reset anyway.
*/
void StackShadow::reset(ADDRINT addr) {
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    unsigned long long shadowPageLimit = (unsigned long long) (shadow[shadowIdx] + SHADOW_ALLOCATION - 1);
    unsigned long long bottom = min((unsigned long long) highestShadowAddr, shadowPageLimit);

#ifdef X86
    /*
    ** SP size is 4 bytes and all stack addresses on return will be aligned to 4 bytes boundaries.
    ** If the address is also aligned to 8 bytes boundary (e.g. 0x7fff00) we must reset only the 4 LSB of the current
    ** shadow address and all the shadow addresses above it (corresponding to lower stack addresses), thus leaving the status of the 4 MSB of
    ** current shadow address (corresponding to 0x7fff04 ~ 0x7fff07, in the example) untouched.
    */
    if(addr % 8 == 0){
        uint8_t mask = (uint8_t) (0xff << 4);
        *shadowAddr &= mask;

        if(shadowAddr != shadowPageLimit){
            ++shadowAddr;
            memset(shadowAddr, 0, bottom - (unsigned long long) shadowAddr + 1);
        }
    }
    else{
        memset(shadowAddr, 0, bottom - (unsigned long long) shadowAddr + 1);
    }
#else
    memset(shadowAddr, 0, bottom - (unsigned long long) shadowAddr + 1);
#endif

    for(unsigned i = shadowIdx + 1; i < shadow.size(); ++i){
        if(dirtyPages[i]){
            memset(shadow[i], 0, SHADOW_ALLOCATION);
            dirtyPages[i] = false;
        }
    }
}

set<std::pair<unsigned, unsigned>> ShadowBase::computeIntervals(uint8_t* uninitializedInterval, ADDRINT accessAddr, UINT32 accessSize){
    set<std::pair<unsigned, unsigned>> ret;

    uint8_t* addr = uninitializedInterval;
    unsigned offset = accessAddr % 8;
    UINT32 size = accessSize + offset;
    UINT32 shadowSize = (size % 8 != 0 ? (size / 8) + 1 : (size / 8));
    uint8_t mask = size % 8 == 0 ? 0 : 0xff << (size % 8);

    while(shadowSize > 0){
        uint8_t val = *addr;
        val |= mask;
        val |= (shadowSize != 1 ? 0 : ((1 << offset) - 1));
        mask = 0;

        if(val != 0xff){
            // Start scanning the bits to find the lower and upper bounds of the interval
            mask = 1 << 7;
            int counter = 0;

            // Starting from the most significant bit, scan every other bit until a 0 is found (guaranteed to be found, 
            // as we already know |val| is not 0xff
            while((val & mask) != 0){
                ++counter;
                mask >>= 1;
            }

            int upperBound = shadowSize * 8 - counter - 1 - offset;

            while((val & mask) == 0 && mask != 0){
                ++counter;
                mask >>= 1;
            }

            // If another 1 is found inside the same shadow byte, we have found the interval's lower bound
            if(mask != 0){
                int lowerBound = shadowSize * 8 - counter - offset;
                ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                // Update mask in order to ignore the interval just found
                mask = 0xff << (8 - counter);
            }
            // the lower bound is in the following shadow byte. Before scanning bit by bit, try to see if the whole
            // shadow byte is 0. If it's not, scan the byte one bit at a time.
            else{
                bool lowerBoundFound = false;

                while(!lowerBoundFound){
                    --shadowSize;
                    ++addr;

                    // This condition evaluates to true if there's no bit set to 1 starting from the |upperBound|.
                    // This means that also the very first byte of the access is uninitialized, so |lowerBound| is 0
                    if(shadowSize == 0){
                        int lowerBound = 0;
                        ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                        break;
                    }

                    if(shadowSize == 1)
                        offset = accessAddr % 8;

                    val = *addr;
                    val |= (shadowSize != 1 ? 0 : ((1 << offset) - 1));

                    if(val != 0){
                        mask = 1 << 7;
                        counter = 0;

                        // Starting from the most significant bit, scan every other bit until a 1 is found (guaranteed to be found, 
                        // as we already know |val| is not 0
                        while((val & mask) == 0){
                            ++counter;
                            mask >>= 1;
                        }

                        int lowerBound = shadowSize * 8 - counter - offset;
                        ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                        mask = 0xff << (8 - counter);
                        lowerBoundFound = true;
                    }
                }
            }
        }
        // This shadow byte is 0xff. No uninitialized interval can be found
        else{
            // |addr| is increased only if it was 0xff. This way, it is checked again after computing
            // interval's bounds in case there are more "holes" in the same shadow byte 
            // (e.g. shadowByte = 11011001 => intervals are [1,2] and [5, 5], assuming only 1 byte is accessed by the application)
            --shadowSize;
            ++addr;
        }
    }

    return ret;
}

void ShadowBase::setBaseAddr(ADDRINT baseAddr){
    this->baseAddr = baseAddr;
}

ShadowBase* ShadowBase::getPtr(){
    return this;
}

void ShadowBase::freeMemory(){
    for(uint8_t* ptr : shadow){
        munmap(ptr, SHADOW_ALLOCATION);
    }
    shadow.clear();
}

StackShadow::StackShadow(){
    shadow.reserve(5);
    dirtyPages.reserve(5);

    for(int i = 0; i < 2; ++i){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) -1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        shadow.push_back(newMap);
        dirtyPages.push_back(false);
    }

    // The address contained by the stack pointer at the beginning of the application entry point is initialized
    // outside the function itself (i.e. by the loader) but the entry point reads it through a pop instruction.
    // In order to avoid to report a false positive, set as initialized the shadow memory mirroring that address.
    *shadow[0] = STACK_SHADOW_INIT;
    dirtyPages[0] = true;
}

HeapShadow::HeapShadow(HeapEnum type) : heapType(type){
    shadow.reserve(5);
    dirtyPages.reserve(5);
    isSingleChunk = false;

    for(int i = 0; i < 2; ++i){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) -1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        shadow.push_back(newMap);
        dirtyPages.push_back(false);
    }
}

void HeapShadow::setAsSingleChunk(){
    isSingleChunk = true;
}

std::pair<unsigned, unsigned> HeapShadow::getShadowAddrIdxOffset(ADDRINT addr){
    unsigned retOffset = (unsigned) (addr - baseAddr);
    unsigned offset = retOffset;
    offset >>= 3;
    unsigned shadowIdx = offset / SHADOW_ALLOCATION;
    return std::pair<unsigned, unsigned>(shadowIdx, retOffset);
}

uint8_t* HeapShadow::getShadowAddrFromIdx(unsigned* shadowIdxPtr, unsigned offset){    
    unsigned shadowIdx = *shadowIdxPtr;
    
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
    return ret;
}

void HeapShadow::set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    dirtyPages[shadowIdx] = true;

    // The address associated to shadowAddr is an 8-bytes aligned address.
    // However, addr may not be 8-bytes aligned. Consider the 8-bytes aligned address
    // represented by shadowAddr, and adjust operations with the offset of the first considered byte
    // from the beginning of that address.
    unsigned offset = addr % 8;
    UINT32 leftSize = size + offset;
    unsigned shadowSize = leftSize % 8 != 0 ? (leftSize / 8) + 1 : leftSize / 8;
    uint8_t* src = invertBitOrder(data, offset, size);

    for(unsigned i = 0; i < shadowSize; ++i){
        // Reset bits related to our access
        uint8_t mask = 0;
        if(i == 0){
            mask |= (uint8_t) 0xff << (8 - offset);
        }
        if(leftSize % 8 != 0 && i == shadowSize - 1){
            mask |= (uint8_t) 0xff >> (leftSize % 8);
        }
        *shadowAddr &= mask;

        *shadowAddr |= *src;

        if(shadowAddr != shadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++shadowAddr;
        } 
        else{
            ++shadowIdx;
            if(shadowIdx < shadow.size()){
                shadowAddr = shadow[shadowIdx];
            }
            else{
                // This should happen in some rare cases, and in most of these cases just 1 time for a single access.
                // This manages the case where the access is performed between 2 memory pages, and the last shadow memory has not been allocated yet
                // Note that this can't happen in a StackShadow object, as when the shadowMemory address is computed, every required memory page
                // is eventually allocated, because the stack grows towards low addresses, so every byte at an address higher than the start address of the 
                // access will already have an allocated shadow memory page.
                uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if(newMap == (void*) - 1){
                    printf("mmap failed: %s\n", strerror(errno));
                    exit(1);
                }
                shadow.push_back(newMap);
                dirtyPages.push_back(false);
                shadowAddr = newMap;
            }
            dirtyPages[shadowIdx] = true;
        }

        ++src;
    }

    if(shadowAddr > highestShadowAddr)
        highestShadowAddr = shadowAddr;
}

void HeapShadow::set_as_initialized(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

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
        offset = 0;

        if(shadowAddr != shadow[shadowIdx] + SHADOW_ALLOCATION - 1){
            ++shadowAddr;
        } 
        else{
            ++shadowIdx;
            if(shadowIdx < shadow.size()){
                shadowAddr = shadow[shadowIdx];
            }
            else{
                // This should happen in some rare cases, and in most of these cases just 1 time for a single access.
                // This manages the case where the access is performed between 2 memory pages, and the last shadow memory has not been allocated yet
                // Note that this can't happen in a StackShadow object, as when the shadowMemory address is computed, every required memory page
                // is eventually allocated, because the stack grows towards low addresses, so every byte at an address higher than the start address of the 
                // access will already have an allocated shadow memory page.
                uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if(newMap == (void*) - 1){
                    printf("mmap failed: %s\n", strerror(errno));
                    exit(1);
                }
                shadow.push_back(newMap);
                dirtyPages.push_back(false);
                shadowAddr = newMap;
            }
            dirtyPages[shadowIdx] = true;
        }

        leftSize -= 8;
    }
    if(leftSize > 0){
        uint8_t mask = ~(0xff >> (leftSize - offset));
        mask >>= offset;
        *shadowAddr |= mask;
    }

    if(shadowAddr > highestShadowAddr)
        highestShadowAddr = shadowAddr;
}

/*
    Reverse a bitmask |data| for an access with |offset| whose size is |byteSize|
    @param data: Bitmask to be reversed
    @param offset: Offset of the address to be accessed
    @param byteSize: Size in bytes of the accessed memory portion
*/
uint8_t* HeapShadow::invertBitOrder(uint8_t* data, unsigned offset, UINT32 byteSize){
    UINT32 size = byteSize + offset;
    UINT32 shadowSize = size % 8 != 0 ? (size / 8) + 1 : size / 8;

    uint8_t* ret = (uint8_t*) calloc(shadowSize, sizeof(uint8_t));
    uint8_t* srcPtr = data + shadowSize - 1;
    uint8_t* dstPtr = ret;
    for(UINT32 j = 0; j < shadowSize; ++j){
        for(int i = 0; i < 8; ++i){
            *dstPtr <<= 1;
            uint8_t lsb = *srcPtr % 2;
            *dstPtr |= lsb;
            *srcPtr >>= 1;
        }

        ++dstPtr;
        --srcPtr;
    }

    return ret;
}

uint8_t* HeapShadow::getUninitializedInterval(ADDRINT addr, UINT32 size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    bool isUninitialized = false;
    bool requiresNewPages = false;
    unsigned offset = addr % 8;
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
            if(shadowIdx >= shadow.size()){
                isUninitialized = true;
                requiresNewPages = true;
            }
        }

        leftSize -= 8;
    }

    // This may happen if the chunk size is very big and it has no corresponding mmapped page
    // considered as a single chunk (which is unlikely, on Ubuntu at least).
    // This should not happen frequently.
    if(requiresNewPages){
        while(leftSize > 0){
            uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if(newMap == (void*) - 1){
                printf("mmap failed: %s\n", strerror(errno));
                exit(1);
            }
            shadow.push_back(newMap);
            dirtyPages.push_back(false);
            leftSize -= (PAGE_SIZE * 8);
        }
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
        // We return the reversed bit sequence in order to adapt the information
        // about uninitialized byte to be aligned to the order both StackShadow and ShadowRegister
        // use (i.e. MSB to the left; LSB to the right)
        uint8_t* shadow_mem_copy = this->shadow_memory_copy(addr, size);
        uint8_t* ret = this->invertBitOrder(shadow_mem_copy, addr % 8, size);
        free(shadow_mem_copy);
        return ret;
    }
    else
        return NULL;
}

void HeapShadow::reset(ADDRINT addr, size_t size){
    std::pair<unsigned, unsigned> idxOffset = this->getShadowAddrIdxOffset(addr);
    unsigned shadowIdx = idxOffset.first;
    uint8_t* shadowAddr = this->getShadowAddrFromIdx(&shadowIdx, idxOffset.second);

    if(heapType == HeapEnum::MMAP && isSingleChunk){
        // If it is a heap allocated through mmap, it is due to a big allocation request.
        // These kind of requests are very rare, and when they happen it is likely to have a long life.
        // For these reasons, it is simpler to simply remove its shadow memory, so that it also reduces memory usage
        freeMemory();
        return;
    }

    unsigned offset = addr % 8;
    // Remember the ShadowMemory model keeps 1 byte for each 8 bytes of application memory
    size_t freed_size = size / 8;
    size_t reset_size = 0;

    while(reset_size < freed_size){
        // If there's no shadow page corresponding to the memory location being freed, it has never been written.
        // So, there's nothing else to do
        if(shadowIdx >= shadow.size())
            return;
        unsigned long long shadowPageLimit = (unsigned long long) shadow[shadowIdx] + SHADOW_ALLOCATION - 1;
        unsigned long long bottom = min((unsigned long long) shadowAddr + freed_size - reset_size - 1, shadowPageLimit);
        unsigned long long freedBytes = bottom - (unsigned long long) shadowAddr + 1;

        if(offset > 0){
            *shadowAddr &= ~((uint8_t) 0xff >> offset);
            ++shadowAddr;
            // Required to reduce freedBytes (and therefore increase reset_size) to avoid writing too many bytes
            // with the subsequent call to memset
            --freedBytes;
            ++reset_size;
        }

        memset(shadowAddr, 0, freedBytes);
        reset_size += freedBytes;
        if(bottom == shadowPageLimit){
            shadowAddr = shadow[++shadowIdx];
        }
        else{
            shadowAddr = (uint8_t*) ++bottom;
        }

        offset = 0;
    }

    // If the size of the block is not a multiple of 8, we need to reset |blockSize % 8| bits of the shadow memory
    // If the initial offset is not 0, we need to reset also the bytes not yet reset due to the offset
    freed_size = size % 8 + addr % 8;
    uint8_t mask = ~((uint8_t) 0xff >> offset);
    mask |= (uint8_t) 0xff >> freed_size;
    *shadowAddr &= mask;
}


StackShadow stack;
HeapShadow heap(HeapEnum::NORMAL);

ShadowBase* currentShadow;

uint8_t* getShadowAddr(ADDRINT addr){
    return currentShadow->getShadowAddr(addr);
}

void set_as_initialized(ADDRINT addr, UINT32 size){
    currentShadow->set_as_initialized(addr, size);
}

void set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data){
    currentShadow->set_as_initialized(addr, size, data);
}

uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size){
    return currentShadow->getUninitializedInterval(addr, size);
}

ShadowBase* getMmapShadowMemory(ADDRINT index){
    HeapShadow& shadowMemory = mmapShadows.find(index)->second;
    return shadowMemory.getPtr();
}
