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

        // This second shadow memory keeps 1 additional bit for each byte of application memory,
        // telling if that byte has been read by a read access considered uninitialized
        vector<uint8_t*> readShadow;

        // Returns a pair containing the index to be used to retrieve the correct shadow page
        // and an offset required to get the correct address inside that page
        virtual std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) = 0;
        
        // Given the index and the offset returned from the previous function,
        // return the corresponding shadow memory address
        virtual uint8_t* getShadowAddrFromIdx(unsigned shadowIdx, unsigned offset) = 0;
        
        // Given a shadow address, return the corresponding shadow address of the read shadow memory
        // (the one containing information about bytes read by uninitialized read accesses)
        uint8_t* getReadShadowFromShadow(uint8_t* shadowAddr, unsigned shadowIdx);
        
        // Function invoked whenever an uninitialized read is executed.
        // It sets the corresponding read shadow memory area to 1.
        virtual void set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx) = 0;
        
        uint8_t* shadow_memory_copy(ADDRINT addr, UINT32 size);
        unsigned long long min(unsigned long long x, unsigned long long y);

    public:
        uint8_t* getShadowAddr(ADDRINT addr);
        
        // Function invoked whenever a write access is executed.
        // It sets the corresponding shadow memory area to 1 and resets the corresponding
        // read shadow memory area to 0
        virtual void set_as_initialized(ADDRINT addr, UINT32 size) = 0;
        
        virtual uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) = 0;
        
        // Returns true if the access represented by the given address and size (at least one of its bytes, actually)
        // is read by an uninitialized read access
        virtual bool isReadByUninitializedRead(ADDRINT addr, UINT32 size) = 0;
        
        // Resets all the shadow memory addresses above or equal to |addr| to 0.
        // This is invoked on return instructions (i.e. when a stack frame is "freed")
        // or on free invocations.
        virtual void reset(ADDRINT addr) = 0;

        // This takes the shadow memory dump saved in the MemoryAccess object and computes the set of uninitialized
        // intervals from that by scanning its bits. For this reason, this is a quite expensive operation, and the
        // more bytes are accessed by a MemoryAccess, the more expensive it is.
        // However, this is executed only for those uninitialized read accesses saved by the tool (which shouldn't be so
        // many in a well produced program), most of which are usually of a few bytes.
        virtual set<std::pair<unsigned, unsigned>> computeIntervals(uint8_t* uninitializedInterval, ADDRINT accessAddr, UINT32 accessSize) = 0;

        void setBaseAddr(ADDRINT baseAddr);
        ShadowBase* getPtr();
};

class StackShadow : public ShadowBase{
    protected:
        std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) override;
        void set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx) override;
        uint8_t* getShadowAddrFromIdx(unsigned shadowIdx, unsigned offset) override;

    public:
        StackShadow();

        void set_as_initialized(ADDRINT addr, UINT32 size) override;
        uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) override;
        bool isReadByUninitializedRead(ADDRINT addr, UINT32 size) override;
        void reset(ADDRINT addr) override;
        set<std::pair<unsigned, unsigned>> computeIntervals(uint8_t* uninitializedInterval, ADDRINT accessAddr, UINT32 accessSize) override;
};

class HeapShadow : public ShadowBase{
    protected:
        HeapEnum heapType;

        std::pair<unsigned, unsigned> getShadowAddrIdxOffset(ADDRINT addr) override;
        void set_as_read_by_uninitialized_read(unsigned size, uint8_t* shadowAddr, unsigned offset, unsigned shadowIdx) override;
        uint8_t* getShadowAddrFromIdx(unsigned shadowIdx, unsigned offset) override;

    public:
        HeapShadow(HeapEnum type);

        void set_as_initialized(ADDRINT addr, UINT32 size) override;
        uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size) override;
        bool isReadByUninitializedRead(ADDRINT addr, UINT32 size) override;
        void reset(ADDRINT addr) override;
        set<std::pair<unsigned, unsigned>> computeIntervals(uint8_t* uninitializedInterval, ADDRINT accessAddr, UINT32 accessSize) override;
};

extern StackShadow stack;
extern HeapShadow heap;
extern unordered_map<ADDRINT, HeapShadow> mmapShadows;

extern ShadowBase* currentShadow;

uint8_t* getShadowAddr(ADDRINT addr);

void set_as_initialized(ADDRINT addr, UINT32 size);

uint8_t* getUninitializedInterval(ADDRINT addr, UINT32 size);

bool isReadByUninitializedRead(ADDRINT addr, UINT32 size);

void reset(ADDRINT addr);

ShadowBase* getMmapShadowMemory(ADDRINT index);

#endif //SHADOWMEMORY