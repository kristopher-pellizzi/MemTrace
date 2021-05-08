
/*! @file
 *  Pintool thought to keep track of memory accesses and report
 *  memory overlaps on the stack, i.e. memory accesses overlapping (fully or partially)
 *  with the area read by an instruction reading memory which has not been initialized 
 *  in its context. This situation may happen, for instance, whenever a function uses an uninitialized local
 *  variable.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <set>
#include <cstdarg>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <memory>

#include <ctime>

#include "ShadowMemory.h"
#include "AccessIndex.h"
#include "MemoryAccess.h"
#include "SyscallHandler.h"
#include "Optional.h"
#include "HeapType.h"
#include "Platform.h"
#include "MallocHandler.h"

using std::cerr;
using std::string;
using std::endl;
using std::map;
using std::tr1::unordered_set;
using std::unordered_map;
using std::vector;
using std::set;

/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#if defined(TARGET_MAC)
#define MALLOC "_malloc"
#define CALLOC "_calloc"
#define FREE "_free"
#define REALLOC "realloc"
#else
#define MALLOC "malloc"
#define CALLOC "calloc"
#define FREE "free"
#define REALLOC "realloc"
#endif

/* ================================================================== */
// Global variables 
/* ================================================================== */

// stderr stream. This is the stream where the tool prints its messages.
std::ostream * out = &cerr;

ADDRINT textStart;
ADDRINT textEnd;
ADDRINT loadOffset;

ADDRINT lastExecutedInstruction;
unsigned long long executedAccesses;

unordered_map<AccessIndex, unordered_set<MemoryAccess, MemoryAccess::MAHasher>, AccessIndex::AIHasher> memAccesses;

// The following map is used as a temporary storage for write accesses: instead of 
// directly insert them inside |memAccesses|, we insert them here (only 1 for each AccessIndex). Every 
// write access contained here is eventually copied inside |memAccesses| when an uninitialized read is found.
// This way, we avoid storing write accesses that are overwritten by another write to the same address and the same size
// and that have not been read by an uninitialized read, thus saving memory space and execution time when we need to
// find the writes whose content is read by uninitialized reads.
unordered_map<AccessIndex, MemoryAccess, AccessIndex::AIHasher> lastWriteInstruction;

// The following map is used as a temporary storage for write accesses during the execution of malloc.
// This is done because in some cases (e.g. the first malloc call) we can decide whether an address is a heap
// address or not only after the malloc is executed, but at that point, we already skipped all the writes done during
// the malloc itself (e.g. on Linux, with the standard glibc, malloc writes the size of the allocated block and the 
// top_chunk structure, and some of them are read by other functions, for instance free reads the block size).
unordered_map<AccessIndex, MemoryAccess, AccessIndex::AIHasher> mallocTemporaryWriteStorage;

// The following set is needed in order to optimize queries about sets containing
// uninitialized read accesses. If a set contains at least 1 uninitialized read,
// the correspondin AccessIndex object is inserted in the set (implemented as an hash table).
unordered_set<AccessIndex, AccessIndex::AIHasher> containsUninitializedRead;
map<AccessIndex, set<MemoryAccess>> partialOverlaps;

ADDRINT libc_base;

// NOTE: this map is useful only in case of a multi-process/multi-threaded application.
// However, the pintool currently supports only single threaded applications; this map has been
// defined as a future extension of the tool to support multi-threaded applications.
map<THREADID, ADDRINT> threadInfos;

bool entryPointExecuted = false;

// Global variable required as the syscall IP is only retrievable at syscall
// entry point, but it is required at syscall exit point to be added to the
// application's memory accesses.
ADDRINT syscallIP;
bool argIsStackAddr;

// Global variables required to keep track of malloc/calloc/realloc returned pointers
bool mallocCalled = false;
bool freeCalled = false;
ADDRINT mallocRequestedSize = 0;
ADDRINT freeRequestedAddr = 0;
unsigned nestedCalls = 0;
ADDRINT oldReallocPtr = 0;
ADDRINT lowestHeapAddr = -1;
ADDRINT highestHeapAddr = 0;
unordered_map<ADDRINT, size_t> mallocatedPtrs;
unordered_map<ADDRINT, size_t> mmapMallocated;
bool mmapMallocCalled = false;

#ifdef DEBUG
    std::ofstream isReadLogger("isReadLog.log");
    std::ofstream analysisProfiling("MemTrace.profile");
    std::ofstream applicationTiming("appTiming.profile");
#endif

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
// KNOB<type>(MODE, FAMILY, NAME, DEFAULT, DESCRIPTION, PREFIX)
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "./overlaps.bin", "Specify the path of the binary report generated by the tool", "");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr    << "This tool generates 2 overlap reports:" << endl;
    cerr    << "The first is overlaps.log, and contains a set of tables (one for each memory access) "
            << "containing all the instructions accessing the address reported in the table header, with the size "
            << "reported in the header as well." << endl;
    cerr    << "The reported instructions are in execution order" << endl;
    cerr    << "Note that the table is reported only if there is at least an instruction reading uninitialized " 
            << "memory area." << endl << endl; 
    cerr    << "The second report is partialOverlaps.log." << endl;
    cerr    << "Again, the file contains a set of tables. This time, the tables contain all the read accesses performed "
            << "at the address and size specified in the table header and all the write instructions of which the "
            << "aforementioned read accesses read at least 1 byte." << endl;
    cerr    << "This allows the user to determine from where each byte (initialized or uninitialized) of read accesses "
            << "come from by simply inspecting the tables in the report" << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

UINT32 min(UINT32 x, UINT32 y){
    return x <= y ? x : y;
}

// Returns true if the considered instruction is any kind of push instruction.
// Note that this is only useful to correctly compute sp offset of the push instructions as 0, otherwise
// the tool would consider as accessed address the last value in the sp before this instruction
// is performed.
// The size of the access should be correctly detected by PIN according to the type of push instruction.
bool isPushInstruction(OPCODE opcode){
    return
        opcode == XED_ICLASS_PUSH ||
        opcode == XED_ICLASS_PUSHA || 
        opcode == XED_ICLASS_PUSHAD || 
        opcode == XED_ICLASS_PUSHF || 
        opcode == XED_ICLASS_PUSHFD || 
        opcode == XED_ICLASS_PUSHFQ;
}

// Similarly to the previous functions, the following returns true if the considered instruction
// is a call instruction, which pushes on the stack the return address
bool isCallInstruction(OPCODE opcode){
    return
        opcode == XED_ICLASS_CALL_FAR ||
        opcode == XED_ICLASS_CALL_NEAR;
}

// This function can be quite expensive if there are many malloc allocating
// huge memory blocks through mmap.
// However, usually malloc is used to allocate small memory blocks, so there should be very few 
// malloc instructions calling mmap.
HeapType isMmapMallocated(ADDRINT addr){
    for(auto iter = mmapMallocated.begin(); iter != mmapMallocated.end(); ++iter){
        ADDRINT lowest = iter->first;
        ADDRINT highest = lowest + iter->second - 1;
        if(addr >= lowest && addr <= highest)
            return HeapType(HeapEnum::MMAP, lowest);
    }
    return HeapType(HeapEnum::INVALID);
}

HeapType isHeapAddress(ADDRINT addr){
    bool isInHeapRange = addr >= lowestHeapAddr && addr <= highestHeapAddr;
    if(isInHeapRange)
        return HeapType(HeapEnum::NORMAL);
    return isMmapMallocated(addr);
}

bool isStackAddress(THREADID tid, ADDRINT addr, ADDRINT currentSp, OPCODE opcode, AccessType type){
    // If the thread ID is not found, there's something wrong.
    if(threadInfos.find(tid) == threadInfos.end()){
        *out << "Thread id not found" << endl;
        exit(1);
    }

    // NOTE: check on the access type in the predicate is required because a push/call instruction may also 
    // read from a memory area, which can be from any memory section (e.g. stack, heap, global variables...)
    if(type == AccessType::WRITE){
        // If it is a push or call instruction, it surely writes a stack address
        if(isPushInstruction(opcode)){
            return true;
        }

        if(isCallInstruction(opcode))
            return true;
    }

    return addr >= currentSp && addr <= threadInfos[tid];
}

// Utility function to compute the intersection of 2 intervals.
Optional<std::pair<unsigned int, unsigned int>> intervalIntersection(std::pair<unsigned int, unsigned int> int1, std::pair<unsigned int, unsigned int> int2){
    if(int1.first <= int2.first){
        if(int1.second < int2.first)
            return None;
        return std::pair<unsigned int, unsigned int>(int2.first, min(int1.second, int2.second));
    }
    else{
        if(int2.second < int1.first)
            return None;
        return std::pair<unsigned int, unsigned int>(int1.first, min(int1.second, int2.second));
    }
}

// Returns true if the set |s| contains at least a full overlap for AccessIndex |targetAI| which is also an
// uninitialized read access.
// NOTE: this differs from function "containsReadIns" as here the set passed as argument may contain also
// uninitialized read accesses which are only partial overlaps, so we need to explicitly
// check whether they are full or partial overlaps.
bool containsUninitializedFullOverlap(set<PartialOverlapAccess>& s){
    for(auto i = s.begin(); i != s.end(); ++i){
        // NOTE: write accesses can't have the |isUninitializedRead| flag set to true, so
        // the second part of the predicate also skips write accesses.
        if(!i->getIsPartialOverlap() && i->getIsUninitializedRead())
            return true;
        
    }
    return false;
}

bool containsReadIns(const AccessIndex& ai){
    return containsUninitializedRead.find(ai) != containsUninitializedRead.end();
}

// The returned pair contains the bounds of the interval of bytes written
// by the access pointed to by |v_it| which overlap with the access represented by |currentSetAI|
std::pair<unsigned, unsigned>* getOverlappingWriteInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it){
    int overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    if(overlapBeginning < 0)
        overlapBeginning = 0;
    int overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    return new pair<unsigned, unsigned>(overlapBeginning, overlapEnd);
}

// Similar to getOverlappingWriteInterval, but the returned pair contains the bounds of the interval of bytes
// read by the access pointed to by |v_it| which are also considered not initialized and overlap with
// the access represented by |currentSetAI|.
bool hasOverlappingUninitializedInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it, set<std::pair<unsigned, unsigned>>& intervals){
    // If the read access happens at an address lower that the set address, it is of no interest
    if(v_it->getAddress() < currentSetAI.getFirst())
        return false;
    
    ADDRINT overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    ADDRINT overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    ADDRINT overlapSize = overlapEnd - overlapBeginning;
    for(const std::pair<unsigned, unsigned>& p : intervals){
        // Return the part of the partial overlap pointed to by v_it that is uninitialized w.r.t. currentSetAI.
        // If the whole partial overlap is initialized, return NULL 
        Optional<std::pair<unsigned, unsigned>> intersect = intervalIntersection(std::pair<unsigned int, unsigned int>(0, overlapSize), p);
        if(intersect){
            return true;
        }
    }

    return false;
}

namespace tracer{
    // Returns true if the write access pointed to by |writeAccess| writes at least 1 byte that is later read
    // by any uninitialized read access overlapping the access represented by |ai|
    bool isReadByUninitializedRead(set<PartialOverlapAccess>::iterator& writeAccess, set<PartialOverlapAccess>& s, const AccessIndex& ai){
        ADDRINT writeStart = writeAccess->getAddress();
        UINT32 writeSize = writeAccess->getSize();

        // Determine the portion of the write access that overlaps with the considered set
        int overlapBeginning = ai.getFirst() - writeStart;
        if(overlapBeginning < 0)
            overlapBeginning = 0;
        int overlapEnd = min(ai.getFirst() + ai.getSecond() - 1 - writeStart, writeSize - 1);

        // Update writeStart, writeEnd and writeSize to consider only the portion of the write that
        // overlaps the considered set
        writeStart += overlapBeginning;
        writeSize = overlapEnd - overlapBeginning + 1;
        ADDRINT writeEnd = writeStart + writeSize - 1;

        #ifdef DEBUG
            isReadLogger << "[LOG]: 0x" << std::hex << ai.getFirst() << " - " << std::dec << ai.getSecond() << endl;
            isReadLogger << "[LOG]: " << std::hex << writeAccess->getDisasm() << " writes " << std::dec << writeSize << " B @ 0x" << std::hex << writeStart << endl;
        #endif

        set<PartialOverlapAccess>::iterator following = writeAccess;
        ++following;

        unsigned int numOverwrittenBytes = 0;
        bool* overwrittenBytes = new bool[writeSize];

        for(UINT32 i = 0; i < writeSize; ++i){
            overwrittenBytes[i] = false;
        }

        while(following != s.end()){

            ADDRINT folStart = following->getAddress();
            ADDRINT folEnd = folStart + following->getSize() - 1;

            // Following access bounds are outside write access bounds. It can't overlap, skip it.
            if(folStart > writeEnd || folEnd < writeStart){
                ++following;
                continue;
            }

            overlapBeginning = folStart - writeStart;
            if(overlapBeginning < 0)
                overlapBeginning = 0;

            overlapEnd = min(folEnd - writeStart, writeSize - 1);

            // If |following| is an uninitialized read access, check if it reads at least a not overwritten byte of 
            // |writeAccess|
            if(following->getType() == AccessType::READ && following->getIsUninitializedRead() && !following->getIsPartialOverlap()){
                set<std::pair<unsigned, unsigned>> intervals = following->computeIntervals();
                bool uninitializedOverlap = hasOverlappingUninitializedInterval(ai, following, intervals);
                // If the uninitialized read access does not overlap |ai| access, it can overlap |writeAccess|, but it is 
                // of no interest now.
                if(!uninitializedOverlap){
                    ++following;
                    continue;
                }

                #ifdef DEBUG
                    isReadLogger << "[LOG]: " << following->getDisasm() << " reads bytes [" << std::dec << overlapBeginning << " ~ " << overlapEnd << "]" << endl;
                #endif

                // Check if the read access reads any byte that is not overwritten
                // by any other write access
                for(int i = overlapBeginning; i <= overlapEnd; ++i){
                    if(!overwrittenBytes[i]){
                        #ifdef DEBUG
                            isReadLogger << "[LOG]: " << following->getDisasm() << " reads a byte of the write" << endl;
                        #endif
                        // Read access reads a not overwritten byte.
                        return true;
                    }
                }
            }

            // If |following| is a write access, update the byte map overwrittenBytes to true where needed.
            // If |writeAccess| has been completely overwritten, it can't be read by any other access, so return false.
            if(following->getType() == AccessType::WRITE){
                #ifdef DEBUG
                    isReadLogger << "[LOG]: " << following->getDisasm() << " overwrites bytes [" << std::dec << overlapBeginning << " ~ " << overlapEnd << "]" << endl;
                #endif

                for(int i = overlapBeginning; i <= overlapEnd; ++i){
                    if(!overwrittenBytes[i])
                        ++numOverwrittenBytes;
                    overwrittenBytes[i] = true;
                }
                // The write access bytes have been completely overwritten by another write access
                // before any read access could read them.
                if(numOverwrittenBytes == writeSize){
                    #ifdef DEBUG
                        isReadLogger << "[LOG]: write access completely overwritten" << endl << endl << endl;
                    #endif
                    return false;
                }
            }
            
            ++ following;
        }
        // |writeAccess| has not been completely overwritten, but no uninitialized read access reads its bytes.
        #ifdef DEBUG
            isReadLogger << "[LOG]: write access not completely overwritten, but never read" << endl << endl << endl;
        #endif
        return false;
    }
}

// Utility function that dumps all the memory accesses recorded during application's execution
// to a file named memtrace.log
void dumpMemTrace(map<AccessIndex, set<MemoryAccess>> fullOverlaps){
    std::ofstream trace("memtrace.log");
    set<MemoryAccess, MemoryAccess::ExecutionComparator> v;

    for(auto it = fullOverlaps.begin(); it != fullOverlaps.end(); ++it){
        v.insert(it->second.begin(), it->second.end());
    }

    for(auto v_it = v.begin(); v_it != v.end(); ++v_it){
        trace <<
            "0x" << std::hex << v_it->getIP() <<
            "(0x" << v_it->getActualIP() << "): " <<
            v_it->getDisasm() << " " <<
            (v_it->getType() == AccessType::READ ? "R " : "W ") <<
            std::dec << v_it->getSize() << " B @ " << std::hex <<
            "0x" << v_it->getAddress() << endl;

    }

    trace.close();
}

void print_profile(std::ofstream& stream, const char* msg){
    time_t timestamp;
    struct tm* currentTime;

    timestamp = time(NULL);
    currentTime = localtime(&timestamp);
    stream 
        << currentTime->tm_hour << ":" << currentTime->tm_min << ":" << currentTime->tm_sec 
        << " - " << msg << endl;
}

void storeMemoryAccess(const AccessIndex& ai, MemoryAccess& ma){
    static MemoryAccess::MAHasher hasher;
    const auto& overlapSet = memAccesses.find(ai);
    if(overlapSet != memAccesses.end()){
        overlapSet->second.insert(ma);
    }
    else{
        unordered_set<MemoryAccess, MemoryAccess::MAHasher> v;
        v.insert(ma);
        memAccesses[ai] = v;
    }
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

// Malloc related analysis routined
VOID MallocBefore(ADDRINT size){
    if(!entryPointExecuted || size == 0)
        return;

    mallocRequestedSize = size;
    mallocCalled = true;
}

VOID CallocBefore(ADDRINT nmemb, ADDRINT size){
    if(!entryPointExecuted || nmemb == 0 || size == 0)
        return;

    mallocRequestedSize = nmemb * size;
    mallocCalled = true;
}

VOID FreeBefore(ADDRINT addr){
    if(!entryPointExecuted || (void*) addr == NULL)
        return;

    freeCalled = true;
    freeRequestedAddr = addr;
}

VOID FreeAfter(ADDRINT ptr){
    // NOTE: the user is calling function |free|, so, we can assume it surely is a heap address.
    // However, a HeapType object also keeps some other useful information. For instance, it allows us to 
    // know if it is a normal malloc or a mmap and to get the pointer to the correct shadow memory index 
    // (in case it is a mmap malloc)

    ptr = malloc_get_block_beginning(ptr);
    HeapType type = isHeapAddress(ptr);
    bool isInvalidForFree = mallocatedPtrs.find(ptr) == mallocatedPtrs.end();

    // If the program is correct, this should never be the case
    if(!type.isValid() || isInvalidForFree){
        *out << "The program called free on an invalid heap address: 0x" << std::hex << ptr << endl;
        exit(1);
    }

    if(type.isNormal()){
        currentShadow = heap.getPtr();
    }
    else{
        currentShadow = getMmapShadowMemory(type.getShadowMemoryIndex());
    }

    set<std::pair<ADDRINT, size_t>> to_reinit = malloc_mem_to_reinit(ptr, malloc_get_block_size(ptr));
    for(const std::pair<ADDRINT, size_t>& segment : to_reinit){
        // NOTE: at this point, we are sure currentShadow is an instance of HeapShadow, so we can
        // perform the cast.
        static_cast<HeapShadow*>(currentShadow)->reset(segment.first, segment.second);
    }

    mallocatedPtrs[ptr] = 0;

    // This condition evaluates to true if the size requested to malloc
    // was so big that the allocator decided to allocate pages dedicated only to that.
    // In that case, all the pages are deallocated.
    ADDRINT page_start = ptr & ~(PAGE_SIZE - 1);
    if(malloc_get_block_size(ptr) == mmapMallocated[page_start]){
        mmapMallocated[page_start] = 0;
    }
}

VOID ReallocBefore(ADDRINT ptr, ADDRINT size){
    if(!entryPointExecuted || size == 0)
        return;
        
    mallocRequestedSize = size;
    mallocCalled = true;
    oldReallocPtr = ptr;
}

// Called right after malloc or calloc or realloc is executed
VOID MallocAfter(ADDRINT ret)
{   
    if(ret == 0)
        return;

    // If this is a malloc performed by a call to realloc and the returned ptr
    // is different from the previous ptr, the previous ptr has been freed.
    if(oldReallocPtr != 0 && oldReallocPtr != ret){
        FreeAfter(oldReallocPtr);
    }

    ret = malloc_get_block_beginning(ret);

    // This kind of allocation should be the same for every platform
    if(mmapMallocCalled){
        // NOTE: mallocRequestedSize has been overridden by the size passed as an argument to mmap
        ADDRINT page_start = ret & ~(PAGE_SIZE - 1);
        mmapMallocated[page_start] = mallocRequestedSize;
        HeapShadow newShadowMem(HeapEnum::MMAP);
        newShadowMem.setBaseAddr(page_start);
        size_t blockSize = malloc_get_block_size(ret);
        mallocatedPtrs[ret] = blockSize;
        // If the blockSize is equal to the size allocated by mmap, this malloc dedicated the whole allocated memory
        // to a single block.
        if(blockSize == mallocRequestedSize){
            newShadowMem.setAsSingleChunk();
        }
        mmapShadows.insert(std::pair<ADDRINT, HeapShadow>(ret, newShadowMem));
    }
    else {
        HeapType type = isHeapAddress(ret);

        // If we are already aware it is a mmap heap address, it means we don't have to update anything
        // as through the mmap we already know the boundaries of the allocated memory. However, we have to
        // insert the returned pointer inside the |mallocatedPtrs| map, with the size of its block
        if(type.isMmap()){
            mallocatedPtrs[ret] = malloc_get_block_size(ret);
            return;
        }
        
        // Otherwise, it means the malloc returned an address of the main heap, which was not allocated through mmap
        // (e.g. glibc allocates it through sbrk system call). In that case, we may need to update the information
        // about the main heap boundaries
        // NOTE: in case the first address returned by malloc is not contained in the first allocated
        // memory page, the following assignment to lowestHeapAddr may invalidate the whole main heap shadow 
        // memory. This is true only if the allocation happened without using mmap. In that case, everything should 
        // work well
        if(ret < lowestHeapAddr){
            // lowestHeapAddr is set to the start address of the memory page the return address belongs to
            lowestHeapAddr = ret & ~ (PAGE_SIZE - 1);
            heap.setBaseAddr(lowestHeapAddr);
        }
        size_t blockSize = malloc_get_block_size(ret);
        mallocatedPtrs[ret] = blockSize;
        ADDRINT lastByte = malloc_get_main_heap_upper_bound(ret, blockSize);
        if(lastByte > highestHeapAddr)
            highestHeapAddr = lastByte;
    }

    // NOTE: |mallocTemporaryWriteStorage| only contains write accesses happened during the execution of malloc
    // and that resulted to not be a heap address. In some cases, this may happen only because we still
    // have to update information about the heap. In any case, the number of accesses stored here should be small 
    // enough to not compromise performances too much.
    for(auto iter = mallocTemporaryWriteStorage.begin(); iter != mallocTemporaryWriteStorage.end(); ++iter){
        const AccessIndex& ai = iter->first;
        const MemoryAccess& ma = iter->second;
        if(HeapType type = isHeapAddress(ai.getFirst())){
            lastWriteInstruction[ai] = ma;
            if(type.isNormal()){
                currentShadow = heap.getPtr();
            }
            else{
                currentShadow = getMmapShadowMemory(type.getShadowMemoryIndex());
            }
            set_as_initialized(ai.getFirst(), ai.getSecond());
        }
    }
    mallocTemporaryWriteStorage.clear();
}

VOID mallocRet(CONTEXT* ctxt){
    if(!mallocCalled && !freeCalled)
        return;

    if(nestedCalls > 0){
        --nestedCalls;
        return;
    }

    // NOTE: when nestedCalls is 0, only 1 and only 1 among freeCalled and mallocCalled is set.
    // According to which of these is set, we should call either MallocAfter or FreeAfter.
    // In any case, we must reinitialize the flags

    if(mallocCalled && !freeCalled){
        ADDRINT ret = PIN_GetContextReg(ctxt, REG_GAX);
        MallocAfter(ret);
    }

    if(!mallocCalled && freeCalled){
        FreeAfter(freeRequestedAddr);
    }

    oldReallocPtr = 0;
    mallocCalled = false;
    mmapMallocCalled = false;
    freeCalled = false;
    freeRequestedAddr = 0;
}

VOID mallocNestedCall(){
    if(!mallocCalled && !freeCalled)
        return;
    ++nestedCalls;
}





VOID memtrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                UINT32 opcode_arg, bool isFirstVisit)
{
    #ifdef DEBUG
        static std::ofstream mtrace("mtrace.log");
        print_profile(applicationTiming, "Tracing new memory access");
    #endif

    ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    OPCODE opcode = opcode_arg;

    bool isWrite = type == AccessType::WRITE;

    // Only keep track of accesses on the stack, so if it is an access to any other memory
    // section, return immediately.
    if(isStackAddress(tid, addr, sp, opcode, type)){
        currentShadow = stack.getPtr();
    }
    else if(HeapType heapType = isHeapAddress(addr)){
        currentShadow = heapType.isNormal() ? heap.getPtr() : getMmapShadowMemory(heapType.getShadowMemoryIndex());
    }
    // If malloc has been called and it is a write access, save it temporarily. After the malloc completed (and we 
    // can therefore decide which of these writes were done on the heap) the interesting ones are stored as normally.
    else if(mallocCalled && isWrite){
        REG regBasePtr;

        // If the stack pointer register is 64 bits long, than we are on an x86-64 architecture,
        // otherwise (size is 32 bits), we are on x86 architecture.

        // NOTE: intel PIN supports only intel x86 and x86-64 architectures, so there's no other possibility.
        if(REG_Size(REG_STACK_PTR) == 8){
            regBasePtr = REG_GBP;
        }
        else{
            regBasePtr = REG_EBP;
        }
        ADDRINT bp = PIN_GetContextReg(ctxt, regBasePtr);

        std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);
        std::string disasm = ins_disasm ? std::string(*ins_disasm) : std::string();

        // If it is a writing push instruction, it increments sp and writes it, so spOffset is 0
        int spOffset = isPushInstruction(opcode) ? 0 : addr - sp;
        int bpOffset = addr - bp;

        MemoryAccess ma(executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, disasm, currentShadow);
        AccessIndex ai(addr, size);
        mallocTemporaryWriteStorage[ai] = ma;
    }
    else
        return;

    // This is an application instruction
    if(ip >= textStart && ip <= textEnd){
        if(!entryPointExecuted){
            entryPointExecuted = true;
        }

        // Compute the last executed application ip. This is useless when application code is executed, but may be useful
        // to track where a library function has been called or jumped to
        lastExecutedInstruction = ip - loadOffset;
    }
    // This probably is a library function, or, in general, code outside .text section
    else{
        // If the entry point has not been executed yet, ignore any memory access that is performed,
        // as it is not performed by the application, but by the runtime bootstrap
        if(!entryPointExecuted){
            return;
        }
    }

    REG regBasePtr;

    // If the stack pointer register is 64 bits long, than we are on an x86-64 architecture,
    // otherwise (size is 32 bits), we are on x86 architecture.

    // NOTE: intel PIN supports only intel x86 and x86-64 architectures, so there's no other possibility.
    if(REG_Size(REG_STACK_PTR) == 8){
        regBasePtr = REG_GBP;
    }
    else{
        regBasePtr = REG_EBP;
    }
    ADDRINT bp = PIN_GetContextReg(ctxt, regBasePtr);

    std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);
    std::string disasm = ins_disasm ? std::string(*ins_disasm) : std::string();

    // If it is a writing push instruction, it increments sp and writes it, so spOffset is 0
    int spOffset = isPushInstruction(opcode) ? 0 : addr - sp;
    int bpOffset = addr - bp;

    MemoryAccess ma(executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, disasm, currentShadow);
    AccessIndex ai(addr, size);

    #ifdef DEBUG
        mtrace <<
            "0x" << std::hex << ma.getIP() <<
            "(0x" << ma.getActualIP() << "): " <<
            ma.getDisasm() << " " <<
            (ma.getType() == AccessType::READ ? "R " : "W ") <<
            std::dec << ma.getSize() << " B @ " << std::hex <<
            "0x" << ma.getAddress() << endl;
    #endif

    // The following static variables are used in order to verify if a read access has already been tracked with the same
    // conditions (the same writes precedes it in an already tracked read accesses).
    // If that's the case, we probably are inside a loop performing the very same read access more than once.
    static unordered_map<MemoryAccess, unordered_set<size_t>, MemoryAccess::NoOrderHasher, MemoryAccess::Comparator> reportedGroups;
    static MemoryAccess::NoOrderHasher maHasher;

    if(isWrite){
        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing write access");
        #endif

        lastWriteInstruction[ai] = ma;
        set_as_initialized(addr, size);
    }
    else{
        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing read access");
        #endif
        uint8_t* uninitializedInterval = getUninitializedInterval(addr, size);
        #ifdef DEBUG
            print_profile(applicationTiming, "\t\tFinished retrieving uninitialized overlap");
        #endif
        
        if(uninitializedInterval != NULL){
            ma.setUninitializedRead();
            ma.setUninitializedInterval(uninitializedInterval);
            containsUninitializedRead.insert(ai);

            size_t hash = maHasher(ma);
            auto overlapGroup = reportedGroups.find(ma);
            if(overlapGroup == reportedGroups.end()){
                // Store the read access
                storeMemoryAccess(ai, ma);

                for(auto iter = lastWriteInstruction.begin(); iter != lastWriteInstruction.end(); ++iter){

                    const auto& lastWrite = iter->second;
                    
                    // Store the write accesses permanently
                    storeMemoryAccess(iter->first, iter->second);

                    // If the considered uninitialized read access reads any byte of this write,
                    // use it to compute the hash representing the context where the read is happening
                    if(isReadByUninitializedRead(lastWrite.getAddress(), lastWrite.getSize())){
                        hash = maHasher.lrot(hash, 4) ^ maHasher(lastWrite);
                    }
                }

                lastWriteInstruction.clear();

                unordered_set<size_t> s;
                s.insert(hash);
                reportedGroups[ma] = s;
            }
            else{
                vector<std::pair<AccessIndex, MemoryAccess>> writes;

                for(auto iter = lastWriteInstruction.begin(); iter != lastWriteInstruction.end(); ++iter){

                    const auto& lastWrite = iter->second;

                    // It is not sure yet we need to insert these write accesses, we must verify if 
                    // this access has already been stored with the same context
                    writes.push_back(std::pair<AccessIndex, MemoryAccess>(iter->first, iter->second));

                    // Compute the context hash
                    if(isReadByUninitializedRead(lastWrite.getAddress(), lastWrite.getSize())){
                        hash = maHasher.lrot(hash, 4) ^ maHasher(lastWrite);
                    }
                }

                lastWriteInstruction.clear();

                unordered_set<size_t>&  reportedHashes = overlapGroup->second;

                // If this is the first time this read access is happening within this context, store it
                if(reportedHashes.find(hash) == reportedHashes.end()){
                    // Store read access
                    storeMemoryAccess(ai, ma);

                    // Permanently store write accesses
                    for(std::pair<AccessIndex, MemoryAccess>& obj : writes){
                        storeMemoryAccess(obj.first, obj.second);
                    }

                    reportedHashes.insert(hash);
                }
            }
        }
    }    
}

// Procedure call instruction pushes the return address on the stack. In order to insert it as initialized memory
// for the callee frame, we need to first initialize a new frame and then insert the write access into its context.
VOID procCallTrace( THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                    UINT32 opcode, bool isFirstVisit)
{
    if(!entryPointExecuted)
        return;

    currentShadow = stack.getPtr();
    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, isFirstVisit);
}

VOID retTrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                UINT32 opcode, bool isFirstVisit)
{
    if(!entryPointExecuted)
        return;
        
    currentShadow = stack.getPtr();

    // If the input triggers an application vulnerability, it is possible that the return instruction reads an uninitialized
    // memory area. Call memtrace to analyze the read access.
    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, isFirstVisit);

    // Reset the shadow memory of the "freed" stack frame.
    // NOTE: at this point we are sure currentShadow is an instance of StackShadow, so we can perform
    // the cast
    static_cast<StackShadow*>(currentShadow)->reset(addr);    
}


// Given a set of SyscallMemAccess objects, generated by the SyscallHandler on system calls,
// add them to the recorded memory accesses
void addSyscallToAccesses(THREADID tid, CONTEXT* ctxt, set<SyscallMemAccess>& v){
    #ifdef DEBUG
        string* disasm = new string("syscall");
    #else
        string* disasm = NULL;
    #endif

    // Actually, it could be another kind of syscall (e.g. int 0x80)
    // but opcode is simply used to be compared to the push opcode, so 
    // does not make any difference
    OPCODE opcode = XED_ICLASS_SYSCALL_AMD;
    for(auto i = v.begin(); i != v.end(); ++i){
        memtrace(tid, ctxt, i->getType(), syscallIP, i->getAddress(), i->getSize(), disasm, opcode, false);    
    }
}

VOID onSyscallEntry(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v){
    if(std == SYSCALL_STANDARD_INVALID){
        *out << "Invalid syscall standard. This syscall won't be traced." << endl;
        *out << "This may create false positives." << endl;
        return;
    }

    ADDRINT actualIp = PIN_GetContextReg(ctxt, REG_INST_PTR);
    syscallIP = actualIp;
    ADDRINT sysNum = PIN_GetSyscallNumber(ctxt, std);
    // If this is a call to mmap and a malloc has been called, but not returned yet,
    // this mmap is part of the malloc itself, which is allocating memory pages,
    // probably because the requested size is very high.
    // The requested size is overridden by the size passed as an argument to mmap
    // (which must be a multiple of the page size). This way, we can store the
    // exact allocated size
    if(sysNum == 9 && mallocCalled){
        mmapMallocCalled = true;
        mallocRequestedSize = PIN_GetSyscallArgument(ctxt, std, 1);
    }
    unsigned short argsCount = SyscallHandler::getInstance().getSyscallArgsCount(sysNum);
    vector<ADDRINT> actualArgs;
    for(int i = 0; i < argsCount; ++i){
        ADDRINT arg = PIN_GetSyscallArgument(ctxt, std, i);
        actualArgs.push_back(arg);
    }

    #ifdef DEBUG
        bool lastSyscallReturned = !SyscallHandler::getInstance().init();
        if(!lastSyscallReturned)
            *out << "Current state: " << SyscallHandler::getInstance().getStateName() << "; Reinitializing handler..." << endl << endl;
        *out << endl << "Setting arguments for syscall number " << std::dec << sysNum << endl;
    #else
        SyscallHandler::getInstance().init();
    #endif

    SyscallHandler::getInstance().setSysArgs((unsigned short) sysNum, actualArgs);
}

VOID onSyscallExit(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v){
    if(std == SYSCALL_STANDARD_INVALID){
        *out << "Invalid syscall standard. This syscall won't be traced." << endl;
        *out << "This may create false positives." << endl;
        return;
    }

    ADDRINT sysRet = PIN_GetSyscallReturn(ctxt, std);
    #ifdef DEBUG
        *out << "Setting return value of the syscall" << endl;
    #endif
    SyscallHandler::getInstance().setSysRet(sysRet);
    #ifdef DEBUG
        *out << "Getting system call memory accesses and resetting state" << endl << endl;
    #endif
    set<SyscallMemAccess> accesses = SyscallHandler::getInstance().getReadsWrites();
    addSyscallToAccesses(threadIndex, ctxt, accesses);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Image(IMG img, VOID* v){
    if(IMG_IsMainExecutable(img)){
        *out << "Main executable: " << IMG_Name(img) << endl;
        *out << "Entry Point: 0x" << std::hex << IMG_EntryAddress(img) << endl;
        for(SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)){
            if(!SEC_Name(sec).compare(".text")){
                textStart = SEC_Address(sec);
                textEnd = textStart + SEC_Size(sec) - 1;
                *out << ".text: 0x" << std::hex << textStart << " - 0x" << textEnd << endl;
                break;
            }
        }

        loadOffset = IMG_LoadOffset(img);
    }

    const std::string& name = IMG_Name(img);
    ADDRINT offset = IMG_LoadOffset(img);
    *out << name << " loaded @ 0x" << std::hex << offset << endl;

    if(name.find("libc") != name.npos)
        libc_base = offset;


    // Instrument the malloc() and free() functions
   
    //  Find the allocation functions.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);
        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)MallocBefore,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                    IARG_END);

        RTN_Close(mallocRtn);
    }

    mallocRtn = RTN_FindByName(img, CALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)CallocBefore,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                    IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the realloc() function
    RTN reallocRtn = RTN_FindByName(img, REALLOC);
    if(RTN_Valid(reallocRtn)){
        RTN_Open(reallocRtn);

        RTN_InsertCall(reallocRtn, IPOINT_BEFORE, (AFUNPTR) ReallocBefore,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                        IARG_END);

        RTN_Close(reallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free() to print the input argument value.
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(freeRtn);
    }
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    ADDRINT stackBase = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    threadInfos.insert(std::pair<THREADID, ADDRINT>(tid, stackBase));
    // NOTE: threadInfos is a map created for a future extension to multi-threaded applications.
    // However, for now the tool supports only single threaded applications.
    stack.setBaseAddr(threadInfos[0]);

    #ifdef DEBUG
        print_profile(applicationTiming, "Application started");
    #endif
}

VOID Instruction(INS ins, VOID* v){
    // Prefetch instruction is used to simply trigger memory areas in order to
    // move them to processor's cache. It does not affect program behaviour,
    // but PIN detects it as a memory read operation, thus producing
    // an uninitialized read.
    if(INS_IsPrefetch(ins))
        return;

    
    bool isProcedureCall = INS_IsProcedureCall(ins);
    bool isRet = INS_IsRet(ins);
    UINT32 memoperands = INS_MemoryOperandCount(ins);

    // If it is not an instruction accessing memory, it is of no interest.
    // Return without doing anything else.
    if(memoperands == 0){
        return;
    }
    else{

        #ifdef DEBUG
            std::string* disassembly = new std::string(INS_Disassemble(ins));
        #else
            std::string* disassembly = NULL;
        #endif

        OPCODE opcode = INS_Opcode(ins);

        for(UINT32 memop = 0; memop < memoperands; memop++){
            // Write memory access
            if(INS_MemoryOperandIsWritten(ins, memop) ){
                // Procedure call instruction
                if(isProcedureCall){
                    INS_InsertPredicatedCall(
                        ins,
                        IPOINT_BEFORE,
                        (AFUNPTR) procCallTrace,
                        IARG_THREAD_ID, 
                        IARG_CONTEXT, 
                        IARG_UINT32, AccessType::WRITE, 
                        IARG_INST_PTR, 
                        IARG_MEMORYWRITE_EA, 
                        IARG_MEMORYWRITE_SIZE,
                        IARG_PTR, disassembly, 
                        IARG_UINT32, opcode, 
                        IARG_BOOL, memop == 0,
                        IARG_END
                    );

                    INS_InsertPredicatedCall(
                        ins,
                        IPOINT_BEFORE,
                        (AFUNPTR) mallocNestedCall,
                        IARG_END
                    );
                }
                else{
                    INS_InsertPredicatedCall(
                        ins, 
                        IPOINT_BEFORE, 
                        (AFUNPTR) memtrace, 
                        IARG_THREAD_ID, 
                        IARG_CONTEXT, 
                        IARG_UINT32, AccessType::WRITE, 
                        IARG_INST_PTR, 
                        IARG_MEMORYWRITE_EA, 
                        IARG_MEMORYWRITE_SIZE,
                        IARG_PTR, disassembly, 
                        IARG_UINT32, opcode, 
                        IARG_BOOL, memop == 0,
                        IARG_END
                    ); 
                }          
            }

            // Read memory access
            if(INS_MemoryOperandIsRead(ins, memop)){
                // It is a ret instruction
                if(isRet){
                    INS_InsertPredicatedCall(
                        ins,
                        IPOINT_BEFORE,
                        (AFUNPTR) retTrace,
                        IARG_THREAD_ID,
                        IARG_CONTEXT,
                        IARG_UINT32, AccessType::READ,
                        IARG_INST_PTR,
                        IARG_MEMORYREAD_EA,
                        IARG_MEMORYREAD_SIZE,
                        IARG_PTR, disassembly,
                        IARG_UINT32, opcode,
                        IARG_BOOL, memop == 0,
                        IARG_END
                    );

                    INS_InsertPredicatedCall(
                        ins,
                        IPOINT_BEFORE,
                        (AFUNPTR) mallocRet,
                        IARG_CONTEXT,
                        IARG_END
                    );
                }
                else{
                    INS_InsertPredicatedCall(
                        ins, 
                        IPOINT_BEFORE, 
                        (AFUNPTR) memtrace, 
                        IARG_THREAD_ID, 
                        IARG_CONTEXT, 
                        IARG_UINT32, AccessType::READ, 
                        IARG_INST_PTR, 
                        IARG_MEMORYREAD_EA, 
                        IARG_MEMORYREAD_SIZE, 
                        IARG_PTR, disassembly,
                        IARG_UINT32, opcode,
                        IARG_BOOL, memop == 0,
                        IARG_END
                    );
                }
            }
        }
    }
}

// Given an unordered_map containing all the traced memory accesses, obtain an ordered copy whose order is useful
// to detect partial overlaps
map<AccessIndex, set<MemoryAccess>> getOrderedCopy(unordered_map<AccessIndex, unordered_set<MemoryAccess, MemoryAccess::MAHasher>, AccessIndex::AIHasher> unorderedMap){
    map<AccessIndex, set<MemoryAccess>> ret;

    for(auto iter = unorderedMap.begin(); iter != unorderedMap.end(); ++iter){
        set<MemoryAccess> orderedSet;
        orderedSet.insert(iter->second.begin(), iter->second.end());
        ret[iter->first] = orderedSet;
    }

    return ret;
}

/*!
 * Generate overlap reports.
 * This function is called when the application exits.
 */
VOID Fini(INT32 code, VOID *v)
{   
    std::string reportPath = KnobOutputFile.Value();
    std::ofstream memOverlaps(reportPath.c_str(), std::ios_base::binary);

    map<AccessIndex, set<MemoryAccess>> fullOverlaps = getOrderedCopy(memAccesses);

    #ifdef DEBUG
        print_profile(applicationTiming, "Application exited");
        std::ofstream partialOverlapsLog("partialOverlaps.dbg");
    #endif

    dumpMemTrace(fullOverlaps);

    int regSize = REG_Size(REG_STACK_PTR);
    memOverlaps.write("\x00\x00\x00\x00", 4);
    memOverlaps << regSize << ";";
    memOverlaps.write(reinterpret_cast<const char*>(&libc_base), regSize);
    memOverlaps.write(reinterpret_cast<const char*>(&threadInfos[0]), regSize);

    #ifdef DEBUG
        print_profile(analysisProfiling, "Starting writing full overlaps report");
    #endif

    /*
    The following iterator, and the boolean flag right inside the next "for" loop scope, are
    used in order to optimize the search of partially overlapping accesses happening at an address lower than the
    address of an access set (denoted as "it" in the loop). Without using these 2 values, we would have
    needed to restart the search from fullOverlaps.begin(), which may require more time.
    */
    std::map<AccessIndex, set<MemoryAccess>>::iterator firstPartiallyOverlappingIterator = fullOverlaps.begin();
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = fullOverlaps.begin(); it != fullOverlaps.end(); ++it){
        #ifdef DEBUG
            print_profile(analysisProfiling, "\tConsidering new set");
        #endif

        bool firstPartiallyOverlappingIteratorUpdated = false;
        // Copy all elements in another set ordered by execution order
        set<MemoryAccess, MemoryAccess::ExecutionComparator> v(it->second.begin(), it->second.end());

        // If the set contains at least 1 uninitialized read, write it into the binary report

        // NOTE: the report is written in a binary format, as it should be faster than writing a well formatted
        // textual report. Textual human-readable reports are generated from the binary reports
        // through an external parser.
        if(containsReadIns(it->first)){
            // |tmp| is used as a temporary ADDRINT copy of ADDRINT values we need to copy in the binary report.
            // This is needed because we need to pass a pointer to the write method.
            ADDRINT tmp = it->first.getFirst();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            memOverlaps << it->first.getSecond() << ";";

            for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); v_it++){
                memOverlaps.write((v_it->getIsUninitializedRead() ? "\x0a" : "\x0b"), 1);
                tmp = v_it->getIP();
                memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
                tmp = v_it->getActualIP();
                memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
                memOverlaps << v_it->getDisasm() << ";";
                memOverlaps.write((v_it->getType() == AccessType::WRITE ? "\x1a" :"\x1b"), 1);
                memOverlaps << v_it->getSize() << ";";
                memOverlaps << v_it->getSPOffset() << ";";
                memOverlaps << v_it->getBPOffset() << ";";
                if(v_it->getIsUninitializedRead()){
                    set<std::pair<unsigned, unsigned>> intervals = v_it->computeIntervals();

                    memOverlaps << intervals.size() << ";";
                    for(const std::pair<unsigned, unsigned>& p : intervals){
                        memOverlaps << p.first << ";";
                        memOverlaps << p.second << ";";
                    }
                }
            }

            // End of full overlap entries
            memOverlaps.write("\x00\x00\x00\x01", 4);

            #ifdef DEBUG
                print_profile(analysisProfiling, "\tFilling set's partial overlaps");
            #endif

            // Fill the partial overlaps map

            // Always create a set to any set in the fullOverlaps map. We will add to the set at least all the
            // instructions contained in the fullOverlaps[it->first] set
            set<MemoryAccess>& vect = partialOverlaps[it->first];

            // Insert backward AccessIndex partially overlapping
            std::map<AccessIndex, set<MemoryAccess>>::iterator partialOverlapIterator = firstPartiallyOverlappingIterator;
            ADDRINT accessedAddress = it->first.getFirst();
            ADDRINT lastAccessedByte = partialOverlapIterator->first.getFirst() + partialOverlapIterator->first.getSecond() - 1;
            while(partialOverlapIterator->first != it->first){
                if(lastAccessedByte >= accessedAddress){
                    vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());
                    if(!firstPartiallyOverlappingIteratorUpdated){
                        firstPartiallyOverlappingIteratorUpdated = true;
                        firstPartiallyOverlappingIterator = partialOverlapIterator;
                    }
                }
                ++partialOverlapIterator;
                lastAccessedByte = partialOverlapIterator->first.getFirst() + partialOverlapIterator->first.getSecond() - 1;
            }

            // Insert forward AccessIndex partially overlapping
            ++partialOverlapIterator;
            accessedAddress = partialOverlapIterator->first.getFirst();
            lastAccessedByte = it->first.getFirst() + it->first.getSecond() - 1;
            while(partialOverlapIterator != fullOverlaps.end() && accessedAddress <= lastAccessedByte){
                vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());

                ++partialOverlapIterator;
                accessedAddress = partialOverlapIterator->first.getFirst();
            }
        }
    }
    // End of full overlaps
    memOverlaps.write("\x00\x00\x00\x02", 4);

    #ifdef DEBUG
        print_profile(analysisProfiling, "Starting writing partial overlaps report");
    #endif

    // Write binary report for partial overlaps
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = partialOverlaps.begin(); it != partialOverlaps.end(); ++it){
        #ifdef DEBUG
            print_profile(analysisProfiling, "\tNew set considered");
        #endif
        
        set<PartialOverlapAccess> v = PartialOverlapAccess::convertToPartialOverlaps(it->second, true);
        PartialOverlapAccess::addToSet(v, fullOverlaps[it->first]);

        if(!containsReadIns(it->first)){
            continue;
        }

        ADDRINT tmp = it->first.getFirst();
        
        memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
        memOverlaps << it->first.getSecond() << ";";

        #ifdef DEBUG
            partialOverlapsLog << "===============================================" << endl;
            partialOverlapsLog << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
            partialOverlapsLog << "===============================================" << endl;
        #endif
        
        for(set<PartialOverlapAccess>::iterator v_it = v.begin(); v_it != v.end(); ++v_it){

            // We do not care about partially overlapping read accesses. 
            // They will have their own set where they will be reported together with the write accesses
            // they read bytes from
            if(v_it->getType() == AccessType::READ && v_it->getIsPartialOverlap())
                continue;

            void* uninitializedOverlap = NULL;
            int overlapBeginning = v_it->getAddress() - it->first.getFirst();
            if(overlapBeginning < 0)
                overlapBeginning = 0;

            if(v_it->getType() == AccessType::WRITE && tracer::isReadByUninitializedRead(v_it, v, it->first))
                uninitializedOverlap = getOverlappingWriteInterval(it->first, v_it);
                    
            // If it is a READ access and it is uninitialized and execution reaches this branch,
            // it surely is an uninitialized read fully overlapping with the considered set
            if(v_it->getType() == AccessType::READ && v_it->getIsUninitializedRead()){
                uninitializedOverlap = v_it->getUninitializedInterval();
            }


            #ifdef DEBUG
                if(v_it->getIsPartialOverlap())
                    partialOverlapsLog << "=> ";
                else
                    partialOverlapsLog << "   ";

                if(v_it->getIsUninitializedRead())
                    partialOverlapsLog << "*";

                partialOverlapsLog    
                    << "0x" << std::hex << v_it->getIP() 
                    << " (0x" << v_it->getActualIP() << ")"
                    << ": " << v_it->getDisasm() << "\t" 
                    << (v_it->getType() == AccessType::WRITE ? "W " : "R ") << std::dec << v_it->getSize() << " B "
                    << "@ 0x" << std::hex << v_it->getAddress() << "; (sp " << (v_it->getSPOffset() >= 0 ? "+ " : "- ") << std::dec << llabs(v_it->getSPOffset()) << "); "
                    << "(bp " << (v_it->getBPOffset() >= 0 ? "+ " : "- ") << llabs(v_it->getBPOffset()) << ") ";
                if(uninitializedOverlap != NULL)
                    partialOverlapsLog << "INTERVAL";
                    //partialOverlapsLog << "bytes [" << std::dec << uninitializedOverlap->first << " ~ " << uninitializedOverlap->second << "]";
                else
                    partialOverlapsLog << " {NULL interval} ";
                partialOverlapsLog << endl;
            #endif

            // It's of no interest in this table. If it is an uninitialized read, it will have its own table.
            if(uninitializedOverlap == NULL)
                continue;
            
            memOverlaps.write(v_it->getIsUninitializedRead() ? "\x0a" : "\x0b", 1);
            tmp = v_it->getIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            tmp = v_it->getActualIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            memOverlaps << v_it->getDisasm() << ";";
            memOverlaps.write((v_it->getType() == AccessType::WRITE ? "\x1a" : "\x1b"), 1);
            memOverlaps << v_it->getSize() << ";";
            memOverlaps << v_it->getSPOffset() << ";";
            memOverlaps << v_it->getBPOffset() << ";";

            if(v_it->getIsPartialOverlap()){
                memOverlaps.write("\xab\xcd\xef\xff", 4);
            }

            set<std::pair<unsigned, unsigned>> intervals;

            if(v_it->getType() == AccessType::WRITE){
                std::pair<unsigned, unsigned> interval = *((std::pair<unsigned, unsigned>*) uninitializedOverlap);
                intervals.insert(interval);
            }
            // If it's not a write access, it is necessarily an uninitialized read access
            else{
                intervals = v_it->computeIntervals();
            }

            memOverlaps << intervals.size() << ";";
            for(const std::pair<unsigned, unsigned>& p : intervals){
                memOverlaps << p.first << ";";
                memOverlaps << p.second << ";";
            }
        }

        memOverlaps.write("\x00\x00\x00\x03", 4);

        #ifdef DEBUG
            partialOverlapsLog << "===============================================" << endl;
            partialOverlapsLog << "===============================================" << endl << endl << endl << endl << endl;
        #endif
    }

    #ifdef DEBUG
        print_profile(analysisProfiling, "Finished");
        analysisProfiling.close();
    #endif

    memOverlaps.write("\x00\x00\x00\x04", 4);

    memOverlaps.close();
    #ifdef DEBUG
        partialOverlapsLog.close();
        isReadLogger.close();
    #endif
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    // Add required instrumentation routines
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Add system call handling routines
    PIN_AddSyscallEntryFunction(onSyscallEntry, NULL);
    PIN_AddSyscallExitFunction(onSyscallExit, NULL);
    
    cerr    <<  "===============================================" << endl;
    cerr    <<  "This application is instrumented by MemTrace." << endl;
    cerr    <<  "This tool produces a binary file. Launch script binOverlapParser.py to generate "
            <<  "the final human-readable reports." << endl;
    cerr    <<  "See files overlaps.log and partialOverlaps.log for analysis results" << endl;
    cerr    <<  "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}