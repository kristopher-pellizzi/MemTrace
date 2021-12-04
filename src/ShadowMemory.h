#ifndef SHADOWMEMORY
#define SHADOWMEMORY

#include <map>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <set>

#include "pin.H"
#include "HeapEnum.h"
#include "Platform.h"

using std::map;
using std::vector;
using std::set;
using std::unordered_map;

extern map<THREADID, ADDRINT> threadInfos;
extern ADDRINT lowestHeapAddr;
extern unordered_map<ADDRINT, size_t> mallocatedPtrs;
extern unordered_map<ADDRINT, size_t> mmapMallocated;

class ShadowBase{
    protected:
        uint8_t* highestShadowAddr;
        ADDRINT baseAddr;

        // This vector contains a bool for each allocated page of shadow memory,
        // specifying whether it is "dirty" (i.e. we have changed its bits) or not
        vector<bool> dirtyPages;

        // This shadow memory keeps 1 bit for each byte of application memory,
        // telling if that byte has been initialized by a write access
        vector<uint8_t*> shadow;

        // Returns a pair containing the index to be used to retrieve the correct shadow page
        // and an offset required to get the correct address inside that page
        virtual std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) = 0;
        
        // Given the index and the offset returned from the previous function,
        // return the corresponding shadow memory address
        virtual uint8_t* getShadowAddrFromIdx(unsigned* shadowIdxPtr, unsigned offset) = 0;
        
        uint8_t* shadow_memory_copy(ADDRINT addr, UINT32 size);
        unsigned long long min(unsigned long long x, unsigned long long y);

    public:
        uint8_t* getShadowAddr(ADDRINT addr);
        
        // Function invoked whenever a write access is executed.
        // It sets the corresponding shadow memory area to 1 or according to |data|.
        virtual void set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data) = 0;

        virtual void set_as_initialized(ADDRINT addr, UINT32 size) = 0;
        
        virtual uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) = 0;

        // This takes the shadow memory dump saved in the MemoryAccess object and computes the set of uninitialized
        // intervals from that by scanning its bits. For this reason, this is a quite expensive operation, and the
        // more bytes are accessed by a MemoryAccess, the more expensive it is.
        // However, this is executed only for those uninitialized read accesses saved by the tool (which shouldn't be so
        // many in a well produced program), most of which are usually of a few bytes.
        set<std::pair<unsigned, unsigned>> computeIntervals(uint8_t* uninitializedInterval, ADDRINT accessAddr, UINT32 accessSize);

        void setBaseAddr(ADDRINT baseAddr);
        ShadowBase* getPtr();
        void freeMemory();
};

class StackShadow : public ShadowBase{
    protected:
        std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) override;
        uint8_t* getShadowAddrFromIdx(unsigned* shadowIdxPtr, unsigned offset) override;

    public:
        StackShadow();

        // Resets all the shadow memory addresses above or equal to |addr| to 0.
        // This is invoked on return instructions (i.e. when a stack frame is "freed")
        // or on free invocations.
        void reset(ADDRINT addr);

        // This function expects as a parameter a buffer of bytes that already includes both the offset and
        // the possible additional bits on the left due to the remainder of 8 of (size + offset). 
        // Of course, to avoid modifying other memory portions than the one we are supposed to update, both the offset 
        // and the additional bits must be set to 0.
        // @param addr: Initial address of the memory portion to write
        // @param size: Size of the memory access
        // @param data: Bitmask representing information about uninitialized bytes
        void set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data) override;
        void set_as_initialized(ADDRINT addr, UINT32 size) override;
        uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) override;
};

class HeapShadow : public ShadowBase{
    protected:
        HeapEnum heapType;
        bool isSingleChunk;

        std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) override;
        uint8_t* getShadowAddrFromIdx(unsigned* shadowIdxPtr, unsigned offset) override;
        uint8_t* invertBitOrder(uint8_t* data, unsigned offset, UINT32 byteSize);

    public:
        HeapShadow(HeapEnum type);

        void setAsSingleChunk();
        void reset(ADDRINT addr, size_t size);

        // This function expects as a parameter a buffer of bytes that already includes both the offset and
        // the possible additional bits on the left due to the remainder of 8 of (size + offset). 
        // Of course, to avoid modifying other memory portions than the one we are supposed to update, both the offset 
        // and the additional bits must be set to 0.
        // @param addr: Initial address of the memory portion to write
        // @param size: Size of the memory access
        // @param data: Bitmask representing information about uninitialized bytes
        void set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data) override;
        void set_as_initialized(ADDRINT addr, UINT32 size) override;
        uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) override;
};

extern StackShadow stack;
extern HeapShadow heap;
extern unordered_map<ADDRINT, HeapShadow> mmapShadows;
extern unsigned long mmapShadowsCounter;

extern ShadowBase* currentShadow;

uint8_t* getShadowAddr(ADDRINT addr);

void set_as_initialized(ADDRINT addr, UINT32 size);

void set_as_initialized(ADDRINT addr, UINT32 size, uint8_t* data);

uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size);

ShadowBase* getMmapShadowMemory(ADDRINT index);

#endif //SHADOWMEMORY