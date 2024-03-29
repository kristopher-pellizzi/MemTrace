
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
#include <list>
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
#include "KnobTypes.h"
#include "ShadowRegisterFile.h"
#include "InstructionHandler.h"
#include "misc/PendingReads.h"
#include "misc/SetOps.h"
#include "misc/DstRegsChecker.h"
#include "misc/InstructionClassification.h"
#include "TagManager.h"
#include "PendingDirectMemoryCopy.h"
#include "XsaveHandler.h"
#include "StackAllocation.h"

using std::cerr;
using std::string;
using std::endl;
using std::map;
using std::tr1::unordered_set;
using std::unordered_map;
using std::vector;
using std::list;
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

// This list is thought to contain all the strings dynamically allocated to contain the instructions disassembly.
// This is required in order to avoid copying a local copy of the whole string into each MemoryAccess object (which simply stores a copy of the 
// pointer) and simultaneously allows us to free them all at the end of the analysis avoiding double delete on the pointers 
// NOTE: more than 1 MemoryAccess object may contain the same pointer, because Intel PIN uses an instruction cache to re-instrument instructions
// that it recently instrumented (e.g. in loops)
std::list<std::string*> disasmPtrs;
std::list<std::list<REG>*> regsPtrs;

bool heuristicEnabled = false;
bool heuristicLibsOnly = false;
bool heuristicAlreadyApplied = false;

bool ignoreLdInstructions;

ADDRINT textStart;
ADDRINT textEnd;
ADDRINT loadOffset;
ADDRINT loaderBaseAddr = -1;
ADDRINT loaderHighestAddr = -1;

ADDRINT lastExecutedInstruction;
StackAllocation lastStackAllocation;
unsigned long long executedAccesses;

PendingDirectMemoryCopy pendingDirectMemoryCopy;

unordered_map<AccessIndex, unordered_set<MemoryAccess, MemoryAccess::MAHasher>, AccessIndex::AIHasher> memAccesses;

// The following map is used as a temporary storage for write accesses: instead of 
// directly insert them inside |memAccesses|, we insert them here (only 1 for each AccessIndex). Every 
// write access contained here is eventually copied inside |memAccesses| when an uninitialized read is found.
// This way, we avoid storing write accesses that are overwritten by another write to the same address and the same size
// and that have not been read by an uninitialized read, thus saving memory space and execution time when we need to
// find the writes whose content is read by uninitialized reads.
map<AccessIndex, MemoryAccess, AccessIndex::LastAccessedByteSorter> lastWriteInstruction;

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

// Map thought to contain the loaded images (e.g. libraries) base addresses, so that it is possible to add them to the report
// in order to make debugging and verification easier
map<std::string, ADDRINT> imgs_base;

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
bool removedThroughBrk = false;
bool memalignCalled = false;
void** memalignPtr = NULL;
ADDRINT mallocRequestedSize = 0;
ADDRINT freeRequestedAddr = 0;
ADDRINT freeBlockSize = 0;
unsigned nestedCalls = 0;
ADDRINT oldReallocPtr = 0;
ADDRINT lowestHeapAddr = -1;
ADDRINT highestHeapAddr = 0;
unordered_map<ADDRINT, size_t> mallocatedPtrs;
unordered_map<ADDRINT, size_t> mmapMallocated;
bool mmapMallocCalled = false;
bool firstMallocCalled = false;

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
KNOB<string> KnobHeuristicStatus(KNOB_MODE_WRITEONCE, "pintool", "u", "LIBS", "Specify whether the string optimization removal heuristic should be enabled", "");
KNOB<bool> KnobKeepLoader(KNOB_MODE_WRITEONCE, "pintool", "-keep-ld", "false", "If enabled, instructions from the loader's library (ld.so in Linux) are not ignored", "");

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

bool isLoaderInstruction(ADDRINT addr){
    /*
    If no loader library has been loaded, the instruction can't be
    a loader instruction
    */
    if(loaderBaseAddr == loaderHighestAddr)
        return false;

    return addr >= loaderBaseAddr && addr <= loaderHighestAddr;
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

    return addr >= currentSp - STACK_REDZONE_SIZE && addr <= threadInfos[tid];
}

// Returns true if the set |s| contains at least a full overlap for AccessIndex |targetAI| which is also an
// uninitialized read access.
// NOTE: this differs from function "containsReadIns" as here the set passed as argument may contain also
// uninitialized read accesses which are only partial overlaps, so we need to explicitly
// check whether they are full or partial overlaps.
// Function's complexity is linear in the size of the given set. However, this function is executed after the program terminates, only once for each
// partial overlaps set.
bool containsUninitializedFullOverlap(set<PartialOverlapAccess>& s){
    for(auto i = s.begin(); i != s.end(); ++i){
        // NOTE: write accesses can't have the |isUninitializedRead| flag set to true, so
        // the second part of the predicate also skips write accesses.
        if(!i->getIsPartialOverlap() && i->getIsUninitializedRead())
            return true;
        
    }
    return false;
}

// This function takes advantage of a hash table, so that it has, on average, a constant complexity
bool containsReadIns(const AccessIndex& ai){
    return containsUninitializedRead.find(ai) != containsUninitializedRead.end();
}

// The returned pair contains the bounds of the interval of bytes written
// by the access pointed to by |v_it| which overlap with the access represented by |currentSetAI|.
// Note that this function allocates dynamically the new pair and returns its pointer.
// This allocation will be deleted afterwards, when the tool writes the binary report.
std::pair<unsigned, unsigned>* getOverlappingWriteInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it){
    int overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    if(overlapBeginning < 0)
        overlapBeginning = 0;
    int overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    return new pair<unsigned, unsigned>(overlapBeginning, overlapEnd);
}

// Returns true if the write access pointed to by |writeAccess| writes at least 1 byte that is later read
// by the uninitialized read pointed to by |readAccess|.
// The function is quite complex (linear w.r.t. the number of accesses stored), as it requires to scan every access starting from |writeAccess| until |readAccess| (note that they
// are in execution order). 
// As soon as we find out the write has been completely overwritten before the read is reached, the function returns false.
// If when we reach the read, it reads at least 1 byte written by the write and never overwritten, it returns true.
// If the read only reads bytes that have been overwritten, but the write is not completely overwritten, false is returned anyway.
bool isReadByUninitializedRead(set<PartialOverlapAccess>::iterator& writeAccess, set<PartialOverlapAccess>::iterator& readAccess){
    ADDRINT writeStart = writeAccess->getAddress();
    UINT32 writeSize = writeAccess->getSize();

    // Determine the portion of the write access that overlaps with the considered set
    int overlapBeginning = readAccess->getAddress() - writeStart;
    if(overlapBeginning < 0)
        overlapBeginning = 0;
    int overlapEnd = min(readAccess->getAddress() + readAccess->getSize() - 1 - writeStart, writeSize - 1);

    // Update writeStart, writeEnd and writeSize to consider only the portion of the write that
    // overlaps the considered set
    writeStart += overlapBeginning;
    writeSize = overlapEnd - overlapBeginning + 1;
    ADDRINT writeEnd = writeStart + writeSize - 1;

    #ifdef DEBUG
        isReadLogger << "[LOG]: 0x" << std::hex << readAccess->getAddress() << " - " << std::dec << readAccess->getSize() << endl;
        isReadLogger << "[LOG]: " << std::hex << writeAccess->getDisasm() << " writes " << std::dec << writeSize << " B @ 0x" << std::hex << writeStart << endl;
    #endif

    set<PartialOverlapAccess>::iterator following = writeAccess;
    ++following;

    unsigned int numOverwrittenBytes = 0;
    bool* overwrittenBytes = new bool[writeSize];

    for(UINT32 i = 0; i < writeSize; ++i){
        overwrittenBytes[i] = false;
    }

    set<PartialOverlapAccess>::iterator end = readAccess;
    ++end;
    while(following != end){

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

        // If |following| is the uninitialized read access, check if it reads at least a not overwritten byte of 
        // |writeAccess|
        if(following == readAccess){
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
                    delete[] overwrittenBytes;
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
                delete[] overwrittenBytes;
                return false;
            }
        }
        
        ++ following;
    }
    // |writeAccess| has not been completely overwritten, but no uninitialized read access reads its bytes.
    #ifdef DEBUG
        isReadLogger << "[LOG]: write access not completely overwritten, but never read" << endl << endl << endl;
    #endif
    delete[] overwrittenBytes;
    return false;
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

void print_profile(std::ostream& stream, const char* msg){
    time_t timestamp;
    struct tm* currentTime;

    timestamp = time(NULL);
    currentTime = localtime(&timestamp);
    stream << std::dec
        << currentTime->tm_hour << ":" << currentTime->tm_min << ":" << currentTime->tm_sec 
        << " - " << msg << endl;
}

void storeMemoryAccess(const AccessIndex& ai, const MemoryAccess& ma){
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

void storeMemoryAccess(set<tag_t>& tags){
    TagManager& tagManager = TagManager::getInstance();

    for(auto iter = tags.begin(); iter != tags.end(); ++iter){
        auto access = tagManager.getAccess(*iter);
        storeMemoryAccess(access.first, access.second);
    }
}

void storeOrLeavePending(OPCODE opcode, AccessIndex& ai, MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs){
    // If it is a mov instruction, it is a simple LOAD, thus leave it pending
    if(isMovInstruction(opcode) || isPushInstruction(opcode) || isPopInstruction(opcode) || shouldLeavePending(opcode)){
        if(dstRegs != NULL)
            InstructionHandler::getInstance().handle(opcode, ma, srcRegs, dstRegs);
        else{
            pendingDirectMemoryCopy = PendingDirectMemoryCopy(ma);
        }
    }
    // If it is anything but a mov instruction, the load is caused by an instruction directly using the loaded value (e.g. add instruction)
    // So, directly store the uninitialized read.
    // Note that this also includes syscall instructions
    else{
        storeMemoryAccess(ai, ma);
        if(dstRegs != NULL)
            // Since we are already reporting this uninitialized read, there's no need to continue propagate these uninitialized bytes
            ShadowRegisterFile::getInstance().setBitsAsInitialized(dstRegs);
    }
}

// Returns true if the uninitialized read is left pending; returns false if the uninitialized read is stored
bool storeOrLeavePending(OPCODE opcode, MemoryAccess& ma, list<REG>* dstRegs, set<tag_t>& tags){
    if(isMovInstruction(opcode) || isPushInstruction(opcode) || isPopInstruction(opcode) || shouldLeavePending(opcode)){
        if(dstRegs != NULL)
            addPendingRead(dstRegs, tags);
        else
            pendingDirectMemoryCopy = PendingDirectMemoryCopy(ma);
        return true;
    }
    else{
        storeMemoryAccess(tags);
        return false;
    }
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID MemalignBefore(void** memptr, size_t size){
    memalignCalled = true;
    memalignPtr = memptr;
    mallocRequestedSize = size;
}

// Malloc related analysis routined
VOID MallocBefore(ADDRINT size){
    if(size == 0)
        return;

    mallocRequestedSize = size;
    mallocCalled = true;
}

VOID CallocBefore(ADDRINT nmemb, ADDRINT size){
    if(nmemb == 0 || size == 0)
        return;

    mallocRequestedSize = nmemb * size;
    mallocCalled = true;
}

VOID FreeBefore(ADDRINT addr){
    if(!entryPointExecuted || (void*) addr == NULL)
        return;

    freeCalled = true;
    freeRequestedAddr = addr;
    // NOTE: it is required to get the block size here, because in some cases the call to free
    // simply overwrites the size of the block with |top_chunk_size| + |block_size|, and sets the top_chunk
    // beginning to the start of the block, thus efficiently re-inserting the block inside
    // the top chunk. Therefore, after the free has been completed, the size of the block is lost, and
    // can't therefore be retrieved in |FreeAfter|.
    // If we get the block size there, and the previously described scenario is going on, a segmentation fault
    // will happen, as we'll try to re-initialize the shadow memory for the whole heap, which might be not allocated
    // yet. In order to avoid segmentation faults, an additional global variable is used to take the block size
    // before the free is actually executed.
    freeBlockSize = malloc_get_block_size(malloc_get_block_beginning(addr));
}

VOID FreeAfter(ADDRINT ptr){
    // NOTE: the user is calling function |free|, so, we can assume it surely is a heap address.
    // However, a HeapType object also keeps some other useful information. For instance, it allows us to 
    // know if it is a normal malloc or a mmap and to get the pointer to the correct shadow memory index 
    // (in case it is a mmap malloc)

    ptr = malloc_get_block_beginning(ptr);
    bool isInvalidForFree = mallocatedPtrs.find(ptr) == mallocatedPtrs.end();

    /*
        If memory has been released through brk system call during the call to free,
        the freed memory location will not exist anymore.
        It would be detected as invalid, so simply remove the allocated chunk from the
        set of allocated memory.
    */
    if(!removedThroughBrk){
        HeapType type = isHeapAddress(ptr);

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

        set<std::pair<ADDRINT, size_t>> to_reinit = malloc_mem_to_reinit(ptr, freeBlockSize);
        for(const std::pair<ADDRINT, size_t>& segment : to_reinit){
            // NOTE: at this point, we are sure currentShadow is an instance of HeapShadow, so we can
            // perform the cast.
            static_cast<HeapShadow*>(currentShadow)->reset(segment.first, segment.second);
        }
    }

    mallocatedPtrs[ptr] = 0;

    // This condition evaluates to true if the size requested to malloc
    // was so big that the allocator decided to allocate pages dedicated only to that.
    // In that case, all the pages are deallocated.
    ADDRINT page_start = ptr & ~(PAGE_SIZE - 1);
    if(freeBlockSize == mmapMallocated[page_start]){
        mmapMallocated[page_start] = 0;
        mmapShadows.erase(page_start);
    }

    if(!removedThroughBrk){
        InstructionHandler& insHandler = InstructionHandler::getInstance();
        for(auto iter = mallocTemporaryWriteStorage.begin(); iter != mallocTemporaryWriteStorage.end(); ++iter){
            const AccessIndex& ai = iter->first;

            ADDRINT accessAddr = ai.getFirst();
            UINT32 accessSize = ai.getSecond();
            
            // If the write access does not overlap (at least partially) with the freed heap chunk,
            // it is not required to set the corresponding shadow memory as initialized.
            // NOTE: this loop should iterate on a map containing very few elements, so it should
            // not degrade performances too much.
            if(
                (accessAddr < ptr && accessAddr + accessSize - 1 < ptr) ||
                (accessAddr > ptr + freeBlockSize - 1)
            ){
                continue;
            }

            insHandler.handle(ai);
        }
    }
    else{
        removedThroughBrk = false;
    }

    if(nestedCalls == 0){
        mallocTemporaryWriteStorage.clear(); 
    }   
}

VOID ReallocBefore(ADDRINT ptr, ADDRINT size){
    if(size == 0)
        return;
        
    mallocRequestedSize = size;
    mallocCalled = true;
    oldReallocPtr = ptr;
    freeBlockSize = malloc_get_block_size(malloc_get_block_beginning(oldReallocPtr));
}

// Called right after malloc or calloc or realloc is executed
VOID MallocAfter(ADDRINT ret)
{   
    if(ret == 0)
        return;

    // If this is a malloc performed by a call to realloc and the returned ptr
    // is different from the previous ptr, the previous ptr has been freed.
    if(oldReallocPtr != 0 && oldReallocPtr != ret){
        // Increasing nestedCalls allows to avoid clearing |mallocTemporaryWriteStorage| during the 
        // call to |FreeAfter|, so that we won't remove from the map writes to be stored 
        // performed during the call to Malloc/Realloc
        ++nestedCalls;
        FreeAfter(oldReallocPtr);
        --nestedCalls;
    }

    size_t blockSize;
    ShadowBase* heapShadow;

    // This kind of allocation should be the same for every platform
    if(mmapMallocCalled){
        // NOTE: mallocRequestedSize has been overridden by the size passed as an argument to mmap
        ADDRINT page_start = ret & ~(PAGE_SIZE - 1);
        std::ostringstream name;
        name << "Heap " << ++mmapShadowsCounter;
        imgs_base.insert(std::pair<string, ADDRINT>(name.str(), page_start));
        ret = malloc_get_block_beginning(ret);

        // This may happen on Linux. Before the entry point is executed, the loader needs to allocate
        // memory pages. When this happen, the allocations are not the same that happen during
        // application execution. These allocated pages won't have a chunk header. In this very specific
        // case, function malloc_get_block_beginning returns a pointer which is lower than the memory page
        // returned by this malloc. We must handle this corner case, because if that happens, the tool
        // will try to access possibly not allocated memory, thus causing a segmentation fault.
        if(ret < page_start){
            mallocTemporaryWriteStorage.clear();
            return;
        }
        blockSize = malloc_get_block_size(ret);
        HeapShadow newShadowMem(HeapEnum::MMAP);
        newShadowMem.setBaseAddr(page_start);
        // If the blockSize is equal to the size allocated by mmap, this malloc dedicated the whole allocated memory
        // to a single block.
        bool isSingleChunk = blockSize == mallocRequestedSize;
        if(isSingleChunk){
            newShadowMem.setAsSingleChunk();
        }

        // We ignore any malloc/free that happens before the entry point is executed, so if this mmap allocates
        // a single chunk, avoid inserting it in any data structure.
        // NOTE: if it is a normal malloc, or a mmap malloc that is not allocated for a single chunk, instead, we need
        // at least to create the corresponding shadow memory page, if it does not exist yet, but we'll
        // set the allocated areas to be ignored as initialized, and we'll never reset them.
        if(!entryPointExecuted && isSingleChunk){
            mallocTemporaryWriteStorage.clear();
            return;
        }
        mmapMallocated[page_start] = mallocRequestedSize;
        mallocatedPtrs[ret] = blockSize;
        auto insertRet = mmapShadows.insert(std::pair<ADDRINT, HeapShadow>(page_start, newShadowMem));
        // Set heapShadow to be the pointer of the just inserted HeapShadow object
        heapShadow = insertRet.first->second.getPtr();
    }
    else {
        ret = malloc_get_block_beginning(ret);
        blockSize = malloc_get_block_size(ret);
        HeapType type = isHeapAddress(ret);

        // If we are already aware it is a mmap heap address, it means we don't have to update anything
        // as through the mmap we already know the boundaries of the allocated memory. However, we have to
        // insert the returned pointer inside the |mallocatedPtrs| map, with the size of its block
        if(type.isMmap()){
            heapShadow = getMmapShadowMemory(type.getShadowMemoryIndex());
        }
        else{
            // If this is a normal malloc, but the brk system call is never called, this is performed before
            // the entry point is actually executed, and won't give a heap pointer, so ignore it
            if(!firstMallocCalled){
                mallocTemporaryWriteStorage.clear();
                return;
            }
            // Otherwise, it means the malloc returned an address of the main heap, which was not allocated through mmap
            // (i.e. through brk system call). In that case, we may need to update the information
            // about the main heap boundaries.
            // NOTE: in case the first address returned by malloc is not contained in the first allocated
            // memory page, the following assignment to lowestHeapAddr may invalidate the whole main heap shadow 
            // memory. This is true only if the allocation happened without using mmap. In that case, everything should 
            // work well
            if(ret < lowestHeapAddr){
                // lowestHeapAddr is set to the start address of the memory page the return address belongs to
                lowestHeapAddr = ret & ~ (PAGE_SIZE - 1);
                heap.setBaseAddr(lowestHeapAddr);
                imgs_base.insert(std::pair<std::string, ADDRINT>("Heap", lowestHeapAddr));
            }

            ADDRINT lastByte = malloc_get_main_heap_upper_bound(ret, blockSize);
            if(lastByte > highestHeapAddr)
                highestHeapAddr = lastByte;

            heapShadow = heap.getPtr();
        }

        mallocatedPtrs[ret] = blockSize;
    }

    // If this malloc happened before the entry point is executed, simply consider it as completely initialized.
    if(!entryPointExecuted){
        currentShadow = heapShadow;

        // Note that when a malloc happens before the entry point, the size of the top_chunk is not set as initialized.
        // This would be easy to fix on glibc implementations, as it would be enough to consider as initialized the 16 bytes
        // immediately following the just allocated chunk (i.e. use blockSize + 16 instead of blockSize).
        // However, this is an implementation specific problem, and may require to be handled differently on different
        // implementations of malloc     
        AccessIndex ai(ret, blockSize);
        InstructionHandler::getInstance().handle(ai);
        return;
    }

    InstructionHandler& insHandler = InstructionHandler::getInstance();

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
            insHandler.handle(ai);
        }
    }
    mallocTemporaryWriteStorage.clear();
}

VOID MemalignAfter(){
    void* returnedPtr;
    PIN_SafeCopy(&returnedPtr, memalignPtr, sizeof(void*));
    if(returnedPtr == NULL){
        return;
    }
    MallocAfter((ADDRINT) returnedPtr);
}

VOID mallocRet(CONTEXT* ctxt){
    if(!mallocCalled && !freeCalled && !memalignCalled)
        return;

    if(nestedCalls > 0){
        --nestedCalls;
        return;
    }

    // NOTE: when nestedCalls is 0, only 1 and only 1 among freeCalled and mallocCalled is set.
    // According to which of these is set, we should call either MallocAfter or FreeAfter.
    // In any case, we must reinitialize the flags

    if(mallocCalled && !freeCalled && !memalignCalled){
        ADDRINT ret = PIN_GetContextReg(ctxt, REG_GAX);
        MallocAfter(ret);
    }

    if(!mallocCalled && freeCalled && !memalignCalled){
        FreeAfter(freeRequestedAddr);
    }

    if(!mallocCalled && !freeCalled && memalignCalled){
        MemalignAfter();
    }

    oldReallocPtr = 0;
    mallocCalled = false;
    memalignCalled = false;
    memalignPtr = NULL;
    mmapMallocCalled = false;
    freeCalled = false;
    freeRequestedAddr = 0;
}

VOID mallocNestedCall(){
    if(!mallocCalled && !freeCalled && !memalignCalled)
        return;
    ++nestedCalls;
}

// STRING OPTIMIZATION REMOVAL HEURISTIC CONDITION EVALUATION FUNCTIONS:
// The following 2 functions compute the conditions to which the uninitialized read access is considered
// to be a consequence of a string optimization and is, therefore, ignored
bool hasOnlyEvenIntervals(set<std::pair<unsigned, unsigned>> intervals){
    for(const auto& interval : intervals){
        if((interval.second - interval.first + 1) % 2 != 0)
            return false;
    }
    return true;
}

bool initUpToNullByte(unsigned nulIndex, set<std::pair<unsigned, unsigned>> intervals){
    auto firstInterval = intervals.begin();

    if(firstInterval->first > nulIndex)
        return true;

    auto secondInterval = intervals.begin();
    ++secondInterval;

    while(secondInterval != intervals.end() && secondInterval->first < nulIndex){
        ++firstInterval;
        ++secondInterval;
    }

    return nulIndex > firstInterval->second;
}


/*
    This analysis function is used in order to store the last stack allocation in a global variable.
    That variable is then used whenever a memory read is executed in order to check whether it is accessing the newly 
    allocated stack space before any write happened, in which case it is considered to be part of the mitigation of the compiler
    against the so-called stack clash vulnerability.
*/
VOID updateLastStackAlloc(CONTEXT* ctxt, VOID* srcRegsPtr, UINT64 immediate){
    list<REG>* srcRegs = static_cast<list<REG>*>(srcRegsPtr);
    UINT64 size = 0;
    ADDRINT startAddr = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    bool requiresProbe = true;

    if(srcRegs->size() == 1){
        size = immediate;
        /*
            When allocation size is an immediate, if it is lower than the page size, it is a tail allocation, which does not require
            a probe.
            If it is higher than a page size, it means the stack clash mitigation is not enabled at all, because with that mitigation enabled,
            the compiler splits any stack allocation higher than a page size in many allocations whose size is exactly a page size + a tail allocation.
        */
        if(size != PAGE_SIZE)
            requiresProbe = false;
    }
    else{
        REG srcReg;
        auto iter = srcRegs->begin();
        while(*iter == REG_STACK_PTR){
            ++iter;
        }
        srcReg = *iter;
        size = PIN_GetContextReg(ctxt, srcReg);
    }

    lastStackAllocation = StackAllocation(startAddr, size, requiresProbe);
}


bool maybeStackClashMitigation(ADDRINT addr){
    if(!lastStackAllocation.requiresProbe())
        return false;

    ADDRINT start = lastStackAllocation.getStartAddr();
    ADDRINT end = start - lastStackAllocation.getSize();
    --start;

    return (addr <= start && addr >= end);
}


VOID memtrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                UINT32 opcode_arg, VOID* srcRegsPtr, VOID* dstRegsPtr)
{
    #ifdef DEBUG
        static std::ofstream mtrace("mtrace.log");
        print_profile(applicationTiming, "Tracing new memory access");
    #endif

    if(size == 0){
        return;
    }
    ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    OPCODE opcode = opcode_arg;

    bool isWrite = type == AccessType::WRITE;

    // Only keep track of accesses on the stack or the heap.
    if(isStackAddress(tid, addr, sp, opcode, type)){
        currentShadow = stack.getPtr();
    }
    else if(HeapType heapType = isHeapAddress(addr)){
        currentShadow = heapType.isNormal() ? heap.getPtr() : getMmapShadowMemory(heapType.getShadowMemoryIndex());
    }
    // If malloc has been called and it is a write access, save it temporarily. After the malloc completed (and we 
    // can therefore decide which of these writes were done on the heap) the interesting ones are stored as normally.
    else if((mallocCalled || memalignCalled) && isWrite){
        lastStackAllocation.unsetRequiresProbeFlag();
        currentShadow = heap.getPtr();

        // NOTE: according to Intel PIN manual, REG_GBP should be register EBP on 32 bit machines, while it is
        // RBP on 64 bit machines.
        REG regBasePtr = REG_GBP;
        ADDRINT bp = PIN_GetContextReg(ctxt, regBasePtr);

        std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);

        // If it is a writing push instruction, it increments sp and writes it, so spOffset is 0
        int spOffset = isPushInstruction(opcode) ? 0 : addr - sp;
        int bpOffset = addr - bp;

        MemoryAccess ma(opcode, executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, ins_disasm, currentShadow);
        AccessIndex ai(addr, size);
        mallocTemporaryWriteStorage[ai] = ma;
        return;
    }
    else{
        // The access is not to the stack, nor to the heap, nor is a write access happening during a malloc
        // Therefore, it is of no interest for the tool
        return;
    }

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

    REG regBasePtr = REG_GBP;
    ADDRINT bp = PIN_GetContextReg(ctxt, regBasePtr);

    std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);

    // If it is a writing push instruction, it increments sp and writes it, so spOffset is 0
    int spOffset = isPushInstruction(opcode) ? 0 : addr - sp;
    int bpOffset = addr - bp;

    MemoryAccess ma(opcode, executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, ins_disasm, currentShadow);
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
    // Note that this is enough to remove most of the duplicated groups of accesses. However, it is possible that some of them
    // are not deleted. We will perform a similar, more precise task after the program's execution terminated.
    static unordered_map<MemoryAccess, unordered_set<size_t>, MemoryAccess::NoOrderHasher, MemoryAccess::Comparator> reportedGroups;
    static MemoryAccess::NoOrderHasher maHasher;
    list<REG>* dstRegs = static_cast<list<REG>*>(dstRegsPtr);
    list<REG>* srcRegs = static_cast<list<REG>*>(srcRegsPtr);

    if(isWrite){
        lastStackAllocation.unsetRequiresProbeFlag();
        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing write access");
        #endif

        lastWriteInstruction[ai] = ma;

        if(pendingDirectMemoryCopy.isValid() && ma.getActualIP() == pendingDirectMemoryCopy.getIp()){
            MemoryAccess& pendingAccess = pendingDirectMemoryCopy.getAccess();
            InstructionHandler::getInstance().handle(pendingAccess, ma, srcRegs);
        }
        // Writes performed by system calls don't have any src register, just store them
        else if(isSyscallInstruction(opcode)){
            InstructionHandler::getInstance().handle(ai);
        }
        else{
            InstructionHandler::getInstance().handle(opcode, ma, srcRegs, dstRegs);
        }

        pendingDirectMemoryCopy.setAsInvalid();
        
        // If this write is performed on the heap by a call to free, we save a copy of it on this ausiliary map.
        // This is useful because when |FreeAfter| will be called, it will re-initialized the shadow memory, including the
        // memory written by the free itself, thus possibly causing many false positives with following malloc calls.
        // To avoid that, after the shadow memory is re-initialized, every write that the free performed on the heap is virtually "re-executed"
        // re-setting the corresponding shadow memory, thus avoiding those aforementioned false positives
        if((oldReallocPtr != 0 || freeCalled || memalignCalled) && !ma.isStackAccess()){
            mallocTemporaryWriteStorage[ai] = ma;
        }
    }
    else{

        // Ignore reads performed during the execution of any variant of a cmp instruction
        if(isCmpInstruction(opcode)){
            return;
        }

        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing read access");
        #endif
        uint8_t* uninitializedInterval = getUninitializedInterval(addr, size);
        #ifdef DEBUG
            print_profile(applicationTiming, "\t\tFinished retrieving uninitialized overlap");
        #endif

        bool pendingReadsExist = pendingUninitializedReads.size() != 0;

        // If the memory read is not an uninitialized read, simply propagate registers status
        if(uninitializedInterval == NULL){
            if(pendingReadsExist)
                // If memory is fully initialized, this handler avoids considering memory at all, thus
                // optimizing performance
                InstructionHandler::getInstance().handle(opcode, srcRegs, dstRegs);
            lastStackAllocation.unsetRequiresProbeFlag();
        }
        else{

            if(maybeStackClashMitigation(addr)){
                lastStackAllocation.unsetRequiresProbeFlag();
                return;
            }

            ma.setUninitializedRead();
            ma.setUninitializedInterval(uninitializedInterval);


            // Check if the loaded value has bytes coming from stored pending reads
            if(storedPendingUninitializedReads.size() != 0){
                range_t memRange(addr, addr + size - 1);
                // [*] Store or leave pending on the dst registers previous pending reads (according if it is a direct usage or a load)
                map<range_t, set<tag_t>> pendingReads = getStoredPendingReads(ma);
                set<tag_t> tags;
                set<range_t, IncreasingStartRangeSorter> ranges;
                ranges.insert(memRange);
                set<pair<unsigned, unsigned>> intervals = ma.computeIntervals();
                set<range_t, IncreasingStartRangeSorter> uninitRanges;
                for(auto iter = intervals.begin(); iter != intervals.end(); ++iter){
                    uninitRanges.insert(range_t(addr + iter->first, addr + iter->second));
                }
                rangeIntersect(ranges, uninitRanges);
    
                for(auto iter = pendingReads.begin(); iter != pendingReads.end(); ++iter){
                    if(!rangeOverlaps(iter->first, ranges))
                        continue;

                    set<tag_t>& iterTags = iter->second;
                    tags.insert(iterTags.begin(), iterTags.end());
                    rangeDiff(ranges, iter->first);
                }

                bool isLeftPending = false;
                if(tags.size() > 0){
                    isLeftPending = storeOrLeavePending(opcode, ma, dstRegs, tags);                    
                } 

                /*
                    The load is uninitialized only because a previous uninitialized read has been stored there.
                    So, it's of no use reporting it, just propagate the status mask into destination registers and return.
                    If this is not the case (and therefore there are uninitialized bytes not coming from previous uninitialized reads)
                    also this read will be reported, and the propagation of status mask to registers will be performed by a later call to |storeOrLeavePending|

                    However, the instructions emulators always add the current read to the dst registers.
                    In order to avoid that, we can set the current read as NOT UNINITIALIZED, so that the status propagation
                    is executed, but the read is not added pending to the dst registers.
                */
                if(ranges.size() == 0){
                    // Only update register status if the uninitialized bytes are left pending on the dst registers.
                    // If, instead, the uninitialized read is stored (because this read is a direct usage) do not update the status
                    if(isLeftPending){
                        ma.setAsInitialized();
                        InstructionHandler::getInstance().handle(opcode, ma, srcRegs, dstRegs);
                    }
                    return;
                }
            }

            // This is an heuristics applied in order to reduce the number of reported uninitialized reads
            // by avoiding reporting those not very significant.
            // More specifically, this is done to try and avoid reporting those uninitialized read
            // accesses performed due to the optimization of strings operations.
            // Being an heuristics, this is not always precise, and may lead to false negatives (e.g. if memcpy is 
            // is implemented using SIMD extensions as well, memcpys may be lost).
            if(heuristicEnabled && size >= 16 && opcode != XED_ICLASS_SYSCALL_AMD && (!heuristicLibsOnly || ip < textStart || ip > textEnd)){
                set<std::pair<unsigned, unsigned>> intervals = ma.computeIntervals();

                if(intervals.size() == 1){
                    std::pair<unsigned, unsigned> interval = *(intervals.begin());
                    bool isCompletelyUninitialized = (interval.first == 0 && interval.second == size - 1);

                    if(isCompletelyUninitialized && heuristicAlreadyApplied){
                        return;
                    }
                }

                heuristicAlreadyApplied = false;

                char* content = (char*) malloc(sizeof(char) * size);
                PIN_SafeCopy(content, (void*) addr, size);

                // Look for an initialized nul byte in the access. If there's no initialized nul byte ('\0'), we
                // will report the access, as it is possible that either this is not a string operation or the 
                // string has not been initialized or a buffer overflow may be happening (e.g. the absence of '\0' in an 
                // uninitialized read may imply the fact that a string terminator is missing, and we are reading 
                // something more than the intended string).
                char* nulPtr = strchr(content, '\0');
                unsigned nulIndex = 0;
                set<uint8_t*> toFree;
            
                while(nulPtr != NULL){
                    nulIndex = nulPtr - content;

                    // A null byte has been found outside the boundaries of the considered access
                    if(nulIndex >= size){
                        nulPtr = NULL;
                        break;
                    }
                    uint8_t* byteShadowCopy = getUninitializedInterval(addr + nulIndex, 1);
                    toFree.insert(byteShadowCopy);
                    
                    // The null byte has been found and has been proven to be initialized
                    if(byteShadowCopy == NULL){
                        break;
                    }
                    nulPtr = strchr(nulPtr + 1, '\0');
                }

                // Free all unused allocated memory
                for(uint8_t* ptr : toFree){
                    free(ptr);
                }

                // At this point, nulPtr points to the first occurrence of '\0' that is also initialized,
                // and nulIndex is the index of that character from the beginning of the considered access
                if(nulPtr != NULL){
                    // If any of these conditions evaluated to true, it is likely to be executin some operation on a string
                    if(
                        !hasOnlyEvenIntervals(intervals) || // There's at least 1 interval with an odd number of uninitialized bytes (note that every other numeric type has at least 2 bytes in C)
                        initUpToNullByte(nulIndex, intervals) // Everything is initialized up to the first initialized null byte '\0'
                    ){
                        free(content);
                        free(uninitializedInterval);
                        heuristicAlreadyApplied = true;
                        return;
                    }
                }
                free(content);
            }

            containsUninitializedRead.insert(ai);


            size_t hash = maHasher(ma);
            auto overlapGroup = reportedGroups.find(ma);

            if(overlapGroup == reportedGroups.end()){
                // Store the read access
                storeOrLeavePending(opcode, ai, ma, srcRegs, dstRegs);

                auto iter = lastWriteInstruction.lower_bound(AccessIndex(ma.getAddress(), 0));
                ADDRINT iterFirstAccessedByte = iter->first.getFirst();
                ADDRINT maFirstAccessedByte = ma.getAddress();
                ADDRINT maLastAccessedByte = maFirstAccessedByte + ma.getSize() - 1;

                while(iter != lastWriteInstruction.end()){
                    
                    // NOTE: according to how the map is sorted, it is not possible that |iterLastAccessedByte| < |maFirstAccessedByte|
                    // However, we still need to check whether |iterFirstAccessedByte| is higher than |maLastAccessedByte|
                    if(iterFirstAccessedByte > maLastAccessedByte){
                        ++iter;
                        iterFirstAccessedByte = iter->first.getFirst();
                        continue;
                    }

                    const auto& lastWrite = iter->second;

                    // If the considered uninitialized read access reads any byte of this write,
                    // use it to compute the hash representing the context where the read is happening
                    hash = maHasher.lrot(hash, 4) ^ maHasher(lastWrite);
                    // Store the write accesses permanently
                    storeMemoryAccess(iter->first, iter->second);

                    ++iter;
                    iterFirstAccessedByte = iter->first.getFirst();
                }

                unordered_set<size_t> s;
                s.insert(hash);
                reportedGroups[ma] = s;
            }
            else{
                vector<std::pair<AccessIndex, MemoryAccess>> writes;

                auto iter = lastWriteInstruction.lower_bound(AccessIndex(ma.getAddress(), 0));
                ADDRINT iterFirstAccessedByte = iter->first.getFirst();
                ADDRINT maFirstAccessedByte = ma.getAddress();
                ADDRINT maLastAccessedByte = maFirstAccessedByte + ma.getSize() - 1;

                while(iter != lastWriteInstruction.end()){

                    // NOTE: according to how the map is sorted, it is not possible that |iterLastAccessedByte| < |maFirstAccessedByte|
                    // However, we still need to check whether |iterFirstAccessedByte| is higher than |maLastAccessedByte|
                    if(iterFirstAccessedByte > maLastAccessedByte){
                        ++iter;
                        iterFirstAccessedByte = iter->first.getFirst();
                        continue;
                    }

                    const auto& lastWrite = iter->second;

                    // Compute the context hash
                    hash = maHasher.lrot(hash, 4) ^ maHasher(lastWrite);
                    // It is not sure yet we need to insert the read access, we must verify if 
                    // it has already been stored with the same context
                    writes.push_back(std::pair<AccessIndex, MemoryAccess>(iter->first, iter->second));

                    ++iter;
                    iterFirstAccessedByte = iter->first.getFirst();
                }

                unordered_set<size_t>&  reportedHashes = overlapGroup->second;

                // If this is the first time this read access is happening within this context, store it
                if(reportedHashes.find(hash) == reportedHashes.end()){
                    // Store read access
                    storeOrLeavePending(opcode, ai, ma, srcRegs, dstRegs);

                    for(std::pair<AccessIndex, MemoryAccess>& write_access : writes){
                        storeMemoryAccess(write_access.first, write_access.second);
                    }

                    reportedHashes.insert(hash);
                }
            }
        }
    }
}

VOID XsaveAnalysis( THREADID tid, CONTEXT* ctxt, ADDRINT ip, ADDRINT addr, UINT32 size,  VOID* disassembly, UINT32 opcode_arg){
    memtrace(tid, ctxt, AccessType::WRITE, ip, addr, size, disassembly, opcode_arg, NULL, NULL);
    
    // If there are no uninitialized registers, it's of no use to bother the XsaveHandler (it might require some time)
    if(pendingUninitializedReads.size() == 0)
        return;

    OPCODE opcode = (OPCODE) opcode_arg;
    ADDRINT eaxContextReg = PIN_GetContextReg(ctxt, REG_GAX);
    uint32_t eaxContent = (uint32_t) eaxContextReg;
    set<AnalysisArgs> s = XsaveHandler::getInstance().getXsaveAnalysisArgs(eaxContent, opcode, addr, size);

    for(auto i = s.begin(); i != s.end(); ++i){
        list<REG>* srcRegs = i->getRegs();
        ADDRINT storeAddr = i->getAddr();
        UINT32 storeSize = i->getSize();
        regsPtrs.push_back(srcRegs);

        memtrace(tid, ctxt, AccessType::WRITE, ip, storeAddr, storeSize, disassembly, opcode_arg, srcRegs, NULL);
    }
}


VOID XrstorAnalysis( THREADID tid, CONTEXT* ctxt, ADDRINT ip, ADDRINT addr, UINT32 size,  VOID* disassembly, UINT32 opcode_arg){
    OPCODE opcode = (OPCODE) opcode_arg;
    ADDRINT eaxContextReg = PIN_GetContextReg(ctxt, REG_GAX);
    uint32_t eaxContent = (uint32_t) eaxContextReg;
    set<AnalysisArgs> s = XsaveHandler::getInstance().getXrstorAnalysisArgs(eaxContent, opcode, addr, size);

    for(auto i = s.begin(); i != s.end(); ++i){
        list<REG>* dstRegs = i->getRegs();
        ADDRINT loadAddr = i->getAddr();
        UINT32 loadSize = i->getSize();
        regsPtrs.push_back(dstRegs);

        memtrace(tid, ctxt, AccessType::READ, ip, loadAddr, loadSize, disassembly, opcode_arg, NULL, dstRegs);
    }
}

// Procedure call instruction pushes the return address on the stack. In order to insert it as initialized memory
// for the callee frame, we need to first initialize a new frame and then insert the write access into its context.
VOID procCallTrace( THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                    UINT32 opcode, VOID* srcRegs, VOID* dstRegs)
{
    if(!entryPointExecuted && (ip < textStart || ip > textEnd)){
        return;
    }

    // The procedure call pushes the return address on the stack
    currentShadow = stack.getPtr();
    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, srcRegs, dstRegs);
}

VOID retTrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                UINT32 opcode, VOID* srcRegs, VOID* dstRegs)
{
    if(!entryPointExecuted){
        return;
    }
        
    // The return instruction, pops the return address from the stack
    currentShadow = stack.getPtr();

    // If the input triggers an application vulnerability, it is possible that the return instruction reads an uninitialized
    // memory area. Call memtrace to analyze the read access.
    heuristicAlreadyApplied = false;
    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, srcRegs, dstRegs);

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
        // Constant complexity insertion
        disasmPtrs.insert(disasmPtrs.end(), disasm);
    #else
        string* disasm = NULL;
    #endif

    // Actually, it could be another kind of syscall (e.g. int 0x80)
    // but opcode is simply used to be compared to the push opcode, so 
    // does not make any difference
    OPCODE opcode = XED_ICLASS_SYSCALL_AMD;
    for(auto i = v.begin(); i != v.end(); ++i){
        memtrace(tid, ctxt, i->getType(), syscallIP, i->getAddress(), i->getSize(), disasm, opcode, NULL, NULL);    
    }
}

/*
    Function used to detect register usage and permanently store all the pending uninitialized
    reads that loaded that register.
    In particular, when a register is read, it will read also the bytes belonging to its sub-registers.
    So, also check if there are uninitialized reads that loaded into its sub-registers and permanently store
    them as well.
*/
VOID checkSourceRegisters(VOID* srcRegs){
    if(!entryPointExecuted || srcRegs == NULL || pendingUninitializedReads.size() == 0)
        return;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    set<MemoryAccess> alreadyInserted;
    list<REG> regs = *static_cast<list<REG>*>(srcRegs);
    set<unsigned> toCheck;

    for(auto iter = regs.begin(); iter != regs.end(); ++iter){

        // If a register is not uninitialized, its subregisters cannot be uninitialized as well
        if(!registerFile.isUninitialized(*iter)){
            continue;
        }
        
        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        unsigned shadowByteSize = registerFile.getByteSize(*iter);
        toCheck.insert(shadowReg);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);

        for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
            // If size of the alias register is lower than the size of the considered register,
            // it is an uninitialized subregister, and we need to check if there's any MemoryAccess object that wrote
            // that subregister
            SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
            if(registerFile.getByteSize(shdw_reg) < shadowByteSize && registerFile.isUninitialized(shdw_reg)){
                toCheck.insert(*aliasReg);
            }
        }
    }

    TagManager& tagManager = TagManager::getInstance();
    for(auto iter = toCheck.begin(); iter != toCheck.end(); ++iter){

        /*
        If the instruction is reading from
        a register previously loaded with an uninitialized read,
        permanently store the read to the memAccesses map, and remove it 
        from the pending reads map.
        */
        auto readIter = pendingUninitializedReads.find(*iter);

        if(readIter != pendingUninitializedReads.end()){
            auto& accessSet = readIter->second;
            for(auto accessIter = accessSet.begin(); accessIter != accessSet.end(); ++accessIter){
                tag_t access_tag = *accessIter;
                const pair<AccessIndex, MemoryAccess>& access = tagManager.getAccess(access_tag);
                // Avoid inserting the same read access more than once (in case it has written more than 1 dst register)
                if(alreadyInserted.find(access.second) == alreadyInserted.end()){
                    storeMemoryAccess(access.first, access.second);
                    alreadyInserted.insert(access.second);
                }
            }
            pendingUninitializedReads.erase(readIter);
            registerFile.setBitsAsInitialized((SHDW_REG) *iter);
        }
    }
}

/*
** Returns true if the syscall corresponding to sysNum is either MMAP or MREMAP, which may be used
** in case of calls to Malloc
*/
bool isMmapOrMremap(ADDRINT sysNum){
    return sysNum == MMAP_NUM || sysNum == MREMAP_NUM;
}

/*
** Returns the size of the heap block allocated through MMAP or MREMAP
*/
ADDRINT getMmapSize(CONTEXT* ctxt, SYSCALL_STANDARD std, ADDRINT sysNum){
    if(sysNum == MMAP_NUM)
        return PIN_GetSyscallArgument(ctxt, std, 1);
    else
        return PIN_GetSyscallArgument(ctxt, std, 2);
}

/* Removes all the writes performed on heap locations that have been removed (by reducing program's 
** break through brk system call).
** Note that this should happen very rarely, because when the program requires to allocated and deallocate
** big chunks on the heap, usually it makes use of mmap syscall.
** Also, this can only happen on the main heap, secondary heaps are usually managed through calls to
** mmap system call.
*/

void removeDeletedMemoryWrites(ADDRINT addr, ADDRINT oldAddr){
    auto iter = lastWriteInstruction.lower_bound(AccessIndex(addr, 0));
    auto firstIter = iter;
    auto lastIter = iter;
    map<AccessIndex, MemoryAccess> toAdd;
    bool isReinitialized = false;

    while(iter != lastWriteInstruction.end()){
        MemoryAccess& ma = iter->second;
        ADDRINT accessStart = ma.getAddress();
        ADDRINT accessEnd = accessStart + ma.getSize() - 1;

        /*
            This write access begins before the deleted portion of the heap, but it has a size such that it
            also wrote some bytes in the deleted part.
            Check if there's another write starting at the same address with a size equal to |ma.size()| - |deletedBytesSize|.
            If there is not, simply add such an access to the map;
            if there is, check the relative order of execution and replace the existing ones only if ma is more recent than the
            existing write access.
        */
        if(accessStart < addr){
            UINT32 deletedSize = accessEnd - addr + 1;
            UINT32 newSize = ma.getSize() - deletedSize;
            AccessIndex newAi(accessStart, newSize);
            auto existingIter = lastWriteInstruction.find(newAi);

            if(existingIter != lastWriteInstruction.end()){
                MemoryAccess& existingMa = existingIter->second;
                /*
                    If ma is more recent than existingMa, replace it; otherwise do nothing
                */
                if(ma.getOrder() > existingMa.getOrder()){
                    MemoryAccess newMa(ma, newSize);
                    toAdd.insert(std::pair<AccessIndex, MemoryAccess>(newAi, newMa));
                }
            }
            else{
                MemoryAccess newMa(ma, newSize);
                toAdd.insert(std::pair<AccessIndex, MemoryAccess>(newAi, newMa));
            }

            heap.reset(addr, deletedSize);
            isReinitialized = true;
        }

        if(accessEnd >= oldAddr){
            
            if(accessStart >= oldAddr){
                ++iter;
                continue;
            }

            /*
                Note that the following code should be executed extremely rarely, as it requires to have reduced
                the main heap through brk (already a rare situation), to have allocated non-heap memory pages right 
                after the main heap memory pages and to have performed a write access partially overlapping both the deleted
                heap portion and the following memory pages.
                This condition is very unlikely to be verified during a program's execution, though it can still happen.
            */
            UINT32 deletedSize = oldAddr - accessStart;
            UINT32 newSize = ma.getSize() - deletedSize;
            AccessIndex newAi(oldAddr, newSize);
            auto existingIter = lastWriteInstruction.find(newAi);

            if(existingIter != lastWriteInstruction.end()){
                MemoryAccess& existingMa = existingIter->second;

                if(ma.getOrder() > existingMa.getOrder()){
                    MemoryAccess newMa(ma, newSize, oldAddr);
                    toAdd.insert(std::pair<AccessIndex, MemoryAccess>(newAi, newMa));
                }
            }
            else{
                    MemoryAccess newMa(ma, newSize, oldAddr);
                    toAdd.insert(std::pair<AccessIndex, MemoryAccess>(newAi, newMa));
            }

            heap.reset(accessStart, deletedSize);
            isReinitialized = true;
        }

        /*
            If the write being removed wrote a portion of memory that has been completely deleted,
            just reinitialize the whole accessed memory.
        */
        if(!isReinitialized){
            heap.reset(accessStart, ma.getSize());
        }

        ++iter;
        lastIter = iter;
    }

    // Method |erase| will remove the interval [firstIter, lastIter), but lastIter is still overlapping
    // the released memory
    ++lastIter;
    lastWriteInstruction.erase(firstIter, lastIter);

    for(auto iter = toAdd.begin(); iter != toAdd.end(); ++iter){
        lastWriteInstruction.insert(*iter);
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
    if(isMmapOrMremap(sysNum) && mallocCalled){
        mmapMallocCalled = true;
        mallocRequestedSize = getMmapSize(ctxt, std, sysNum);
    }

    if(sysNum == BRK_NUM){
        ADDRINT arg = PIN_GetSyscallArgument(ctxt, std, 0);

        if(arg != 0){

            /*
                If main heap size is reduced through a negative sbrk, remove the writes
                accessing the deleted memory.
            */
            if(arg < highestHeapAddr){
                removeDeletedMemoryWrites(arg, highestHeapAddr);

                /*
                    If brk is used to remove heap memory during a call to free, brk will be
                    called before free actually terminates.
                    So, when |FreeAfter| will be called, the freed memory location won't be 
                    available as a heap memory location anymore, thus resulting in an invalid free which
                    terminates the analysis.
                    To avoid this, we set |removedThroughBrk|, so that we can return |FreeAfter| immediately.
                */
                if(freeCalled)
                    removedThroughBrk = true;
            }

            if(mallocCalled || memalignCalled){
                firstMallocCalled = true;
            }

            highestHeapAddr = arg;
        }
    }

    unsigned short argsCount = SyscallHandler::getInstance().getSyscallArgsCount(sysNum);
    vector<ADDRINT> actualArgs;
    list<REG> argRegs;
    for(int i = 0; i < argsCount; ++i){
        ADDRINT arg = PIN_GetSyscallArgument(ctxt, std, i);
        actualArgs.push_back(arg);
        argRegs.push_back(syscall_args[i]);
    }

    // Check if any of the syscall argument registers contain a pending uninitialized read
    checkSourceRegisters(&argRegs);

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

/*
    Analysis function used to detect the overwriting of a register. In practice, every time a register is overwritten we must
    remove all pending uninitialized reads whose uninitialized bytes are overwritten by writing the destination
    registers. For instance, let's suppose an uninitialized read loads rax with uninitialized mask 11111000.
    A subsequent overwrite of register eax with mask 1111 will overwrite the previous load. So, previous uninitialized 
    read won't be reported, because never used.
    If instead only ax was written with mask 11, the previous uninitialized bytes are not completely overwritten and, so,
    the read is not removed. Indeed, if a subsequent instruction would use eax or rax, 1 uninitialized byte loaded by the 
    first load will be read.
*/
VOID checkDestRegistersAnalysis(UINT32 opcode_arg, VOID* dstRegsPtr){
    OPCODE opcode = (OPCODE) opcode_arg;
    list<REG>* dstRegs = static_cast<list<REG>*>(dstRegsPtr);
    auto findIter = checkDestSize.find(opcode);
    if(findIter != checkDestSize.end()){
        unsigned bitSize = findIter->second;
        checkDestRegisters(dstRegs, opcode, bitSize);
    }
    else{
        checkDestRegisters(dstRegs, opcode);
    }
}


VOID propagateRegisterStatus(UINT32 opcodeArg, VOID* srcRegsPtr, VOID* dstRegsPtr){    
    if(!entryPointExecuted || pendingUninitializedReads.size() == 0 || srcRegsPtr == NULL || dstRegsPtr == NULL)
        return;

    list<REG>* srcRegs = static_cast<list<REG>*>(srcRegsPtr);
    list<REG>* dstRegs = static_cast<list<REG>*>(dstRegsPtr);
    OPCODE opcode = static_cast<OPCODE>(opcodeArg);

    InstructionHandler::getInstance().handle(opcode, srcRegs, dstRegs);
}

/*
    This analysis function is simply used to update the last executed instruction in case the instruction is a jmp
    instruction and it is inside the .text section.
    This is required because it is possible that a jmp instruction makes the program jump outside the .text section
    (e.g. inside a library), and if we don't update the last executed instruction, the following memory accesses will be stored
    with an erroneous IP.
*/
VOID updateLastExecutedInstruction(ADDRINT ip){
    lastExecutedInstruction = ip - loadOffset;
}


/*
    The following 2 analysis functions are used in order to emulate the behavior of the FPU register stack without 
    transfering values from a register to another one
*/
VOID decrementFpuStackIndex(){
    ShadowRegisterFile::getInstance().decrementFpuStackIndex();
}


VOID incrementFpuStackIndex(){
    ShadowRegisterFile::getInstance().incrementFpuStackIndex();
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

        loadOffset = IMG_LowAddress(img);
    }

    const std::string& name = IMG_Name(img);
    ADDRINT offset = IMG_LowAddress(img);
    *out << name << " loaded @ 0x" << std::hex << offset << endl;
    /*
    If the image is the loader (library ld.so for Linux), store its address, as it will be needed during analysis
    to ignore its instructions. This is done because the writes performed by the loader, seen by the same uninitialized 
    read, may change between different execution of the same command.
    */
    if(IMG_IsInterpreter(img)){
        loaderBaseAddr = offset;
        loaderHighestAddr = IMG_HighAddress(img);
    }

    imgs_base.insert(std::pair<std::string, ADDRINT>(name, offset));


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

    RTN memalignRtn = RTN_FindByName(img, "posix_memalign");
    if(RTN_Valid(memalignRtn)){
        RTN_Open(memalignRtn);
        RTN_InsertCall( memalignRtn, IPOINT_BEFORE, (AFUNPTR) MemalignBefore,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                        IARG_END);
        RTN_Close(memalignRtn);
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

/*
Return true if the xor instruction is a zeroing xor, i.e. an instruction of type "xor rdi, rdi"
*/
bool isZeroingXor(INS ins, OPCODE opcode, list<REG>* dstRegs, list<REG>* srcRegs){
    if(srcRegs == NULL || dstRegs == NULL)
        return false;

    /*
    If the instruction is not a xor or the xor instruction has only 1 source register 
    (the other operand is an immediate), then it can't be a zeroing xor.
    */
    size_t readRegisters = dstRegs->size() + srcRegs->size();

    if(!isXorInstruction(opcode) || readRegisters <= 2)
        return false;

    auto iter = srcRegs->begin();
    REG firstOperand = *(iter++);
    REG secondOperand = *iter;
    return firstOperand == secondOperand;
}

VOID HandleXsave(INS ins){
    OPCODE opcode = INS_Opcode(ins);
    UINT32 memoperands = INS_MemoryOperandCount(ins);

    #ifdef DEBUG
        std::string* disassembly = new std::string(INS_Disassemble(ins));
        // Constant complexity insertion
        disasmPtrs.insert(disasmPtrs.end(), disassembly);
    #else
        std::string* disassembly = NULL;
    #endif

    if(isXsaveInstruction(opcode)){
        for(UINT32 memop = 0; memop < memoperands; memop++){ 
            if(INS_MemoryOperandIsWritten(ins, memop) ){
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR) XsaveAnalysis,
                    IARG_THREAD_ID,
                    IARG_CONTEXT,
                    IARG_INST_PTR,
                    IARG_MEMORYWRITE_EA,
                    IARG_MEMORYWRITE_SIZE,
                    IARG_PTR, disassembly,
                    IARG_UINT32, opcode,
                    IARG_END
                ); 
            }

        }
    }
    else{
        for(UINT32 memop = 0; memop < memoperands; memop++){
            if(INS_MemoryOperandIsRead(ins, memop)){
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR) XrstorAnalysis,
                    IARG_THREAD_ID,
                    IARG_CONTEXT,
                    IARG_INST_PTR,
                    IARG_MEMORYREAD_EA,
                    IARG_MEMORYREAD_SIZE,
                    IARG_PTR, disassembly,
                    IARG_UINT32, opcode,
                    IARG_END
                );
            }
        }
    }
}

/*
    Effective address computation:
    Effective address = Displacement + BaseReg + IndexReg * Scale

    So an instruction reads the stack top if these conditions hold simultaneously:
    [*] Base register is the stack ptr
    [*] There is no displacement from the base register
    [*] There is no index register or there's the index register, but the scale is 0

    The function checks if any of them does not hold, in which case returns false;
    otherwise it returns true
*/
bool MemoryOperandReadsStackTop(INS ins, UINT32 memop){ 
    if(
        INS_OperandMemoryBaseReg(ins, memop) != REG_STACK_PTR ||
        INS_OperandMemoryDisplacement(ins, memop) != 0 ||
        (REG_valid(INS_OperandMemoryIndexReg(ins, memop)) && INS_OperandMemoryScale(ins, memop) != 0)
    )
        return false;

    return true;
}


/*
    Returns true if the instruction is of type
    SUB rsp, {PAGE_SIZE}.

    This functions checks:
    [*] Opcode is SUB
    [*] The only register operand is the stack pointer
    [*] Immediate is PAGE_SIZE
    [*] There are no operands which are not a register, nor an immediate

    If any of the conditions above does not hold, the function returns false,
    otherwise returns true.
*/
bool isStackAlloc(OPCODE opcode, list<REG>* dstRegs){
    /*
        Stack allocations are performed by simply decrementing the stack pointer
        through a SUB instruction
    */
    if(opcode != XED_ICLASS_SUB){
        return false;
    }

    if(dstRegs == NULL || dstRegs->size() != 1 || *dstRegs->begin() != REG_STACK_PTR)
        return false;

    return true;
}


VOID Instruction(INS ins, VOID* v){
    OPCODE opcode = INS_Opcode(ins);
    INT32 ext = INS_Extension(ins);
    if(isSSEInstruction(ext)){
        sseInstructions.insert(opcode);
    }

    // Prefetch instruction is used to simply trigger memory areas in order to
    // move them to processor's cache. It does not affect program behaviour,
    // but PIN detects it as a memory read operation, thus producing
    // an uninitialized read.
    if(INS_IsPrefetch(ins) || isEndbrInstruction(opcode) || INS_IsNop(ins))
        return;

    /*
        If it is an XSAVE instruction (or one of its variants), it must be specifically handled,
        as Intel PIN won't tell us which registers are stored in memory.
    */
    if(isXsaveInstruction(opcode) || isXrstorInstruction(opcode)){
        HandleXsave(ins);
        return;
    }
    
    bool isProcedureCall = INS_IsProcedureCall(ins);
    bool isRet = INS_IsRet(ins);
    UINT32 operandCount = INS_OperandCount(ins);
    UINT32 memoperands = INS_MemoryOperandCount(ins);
    UINT32 readRegisters = INS_MaxNumRRegs(ins);
    list<REG>* srcRegs = NULL;
    list<REG>* explicitSrcRegs = NULL;
    list<REG>* dstRegs = NULL;
    REG repCountRegister = INS_RepCountRegister(ins);   


    /* 
        Take the explicitly written destination registers.
        We simply ignore the implicit destination registers.
        E.g. consider a push instruction. It does not really write in a register.
        However, it implicitly writes register %rsp, because it must decrement it.
        We are not interested, as we only need the registers potentially loaded with bytes from
        an uninitialized read.
    */
    for(UINT32 op = 0; op < operandCount; ++op){
        if(INS_OperandIsReg(ins, op)){
            REG reg = INS_OperandReg(ins, op);
            if(REG_is_flags(reg) || (REG_valid(repCountRegister) && reg == repCountRegister))
                continue;

            // If the register is explicitly written, add it to the destination registers set
            if(INS_OperandWritten(ins, op)){
                if(dstRegs == NULL){
                    dstRegs = new list<REG>();
                    regsPtrs.push_back(dstRegs);
                }
                dstRegs->push_back(reg);
            }

            // If the register is explicitly read, add it to the explicit source registers set
            if(INS_OperandRead(ins, op)){
                if(explicitSrcRegs == NULL){
                    explicitSrcRegs = new list<REG>();
                    regsPtrs.push_back(explicitSrcRegs);
                }
                explicitSrcRegs->push_back(reg);
            }
        }
    }

    /*
        Take all the registers used as source (even the implicit ones).
        Differently from destination registers, we even consider implicitly read registers because it indicates
        the usage of the register.
        If a register loaded with bytes from an uninitialized read is used, the uninitialized read itself is stored,
        otherwise it is discarded. This is why we need to consider every possible usage of each register.

        However, we are ignoring 2 kind of register usage:
        [*] If the register is used by a cmp instruction, it is not so relevant, as we are interested in
            leaks, not uninitialized memory usage in general
        [*] If the register is used by a zeroing xor, it is not relevant, because the only effect of such kind of an 
            instruction is to set the register to 0, thus throwing away any value it contains.
    */
    if(!isCmpInstruction(opcode)){
        // Add all source registers (both implicit and explicit) to the set of source registers
        for(UINT32 regop = 0; regop < readRegisters; ++regop){
            REG src = INS_RegR(ins, regop);
            if(!REG_is_flags(src)){
                if(srcRegs == NULL){
                    srcRegs = new list<REG>();
                    regsPtrs.push_back(srcRegs);
                }
                srcRegs->push_back(src);
            }
        }
    }

    if(isZeroingXor(ins, opcode, dstRegs, srcRegs)){
        srcRegs->clear();
    }


    // Start inserting analysis functions
    if(isStackAlloc(opcode, dstRegs)){
        UINT64 immediate = 0;

        for(UINT32 i = 0; i < operandCount; ++i){
            if(INS_OperandIsImmediate(ins, i)){
                immediate = INS_OperandImmediate(ins, i);
            }
        }

        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR) updateLastStackAlloc,
            IARG_CONTEXT,
            IARG_PTR, srcRegs,
            IARG_UINT64, immediate,
            IARG_END
        );
    }

    /*
        If it's a FPU push instruction, decrement the fpu stack index before writing the register
    */
    if(isFpuPushInstruction(opcode)){
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR) decrementFpuStackIndex,
            IARG_END
        );
    }

    /*
        If the instruction is a mov or push instruction, avoid considering explicitly read registers as a register 
        usage, because we're simply copying its value to another register or to memory.
        However, we need to check if an implicitly read register is used uninitialized.
        So, in these cases, simply remove all the explicitly read registers from the set of all src registers
        and call |checkSourceRegisters|.

        Examples:
        1)  An uninitialized read loads register rbx. rbx is copied to rcx through a mov. rbx is an explicit src register,
            so it will be removed from the set, and the uninitialized read won't be reported yet.
            If either rbx or rcx is later used, the uninitialized read will be reported.
        2)  An uninitialized read loads register rbx. rbx is then used as a base pointer to load something else through 
            a mov. rbx is an implicitly read register (it's read to compute the address we're reading from), so it is not
            removed from the set, and the uninitialized read is therefore reported.
    */
 
    if(isPushInstruction(opcode) || isMovInstruction(opcode) || shouldLeavePending(opcode)){
        setDiff(srcRegs, explicitSrcRegs);
    }
    INS_InsertPredicatedCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR) checkSourceRegisters,
        IARG_PTR, srcRegs,
        IARG_END
    );

    if(!isAutoMov(opcode, explicitSrcRegs, dstRegs)){
        INS_InsertPredicatedCall(
            ins, 
            IPOINT_BEFORE,
            (AFUNPTR) checkDestRegistersAnalysis,
            IARG_UINT32, opcode,
            IARG_PTR, dstRegs,
            IARG_END
        );
    }

    ADDRINT ip = INS_Address(ins);
    if(INS_IsBranch(ins) && ip >= textStart && ip <= textEnd){
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR) updateLastExecutedInstruction,
            IARG_INST_PTR,
            IARG_END
        );
    }

    // If it is not an instruction accessing memory, it is of no interest.
    // However, we need to propagate information about uninitialized bytes to perform taint analysis.
    if(memoperands == 0){
        INS_InsertPredicatedCall(
            ins, 
            IPOINT_BEFORE,
            (AFUNPTR) propagateRegisterStatus,
            IARG_UINT32, opcode,
            IARG_PTR, explicitSrcRegs,
            IARG_PTR, dstRegs,
            IARG_END
        );
    }
    else{

        #ifdef DEBUG
            std::string* disassembly = new std::string(INS_Disassemble(ins));
            // Constant complexity insertion
            disasmPtrs.insert(disasmPtrs.end(), disassembly);
        #else
            std::string* disassembly = NULL;
        #endif

        set<UINT32> readMemOperands;
        set<UINT32> writtenMemOperands;

        for(UINT32 memop = 0; memop < memoperands; memop++){ 
            // Read memory access
            if(INS_MemoryOperandIsRead(ins, memop)){
                readMemOperands.insert(memop);
            }

            if(INS_MemoryOperandIsWritten(ins, memop)){
                writtenMemOperands.insert(memop);
            }
        }

        for(auto i = readMemOperands.begin(); i != readMemOperands.end(); ++i){
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
                    IARG_PTR, explicitSrcRegs,
                    IARG_PTR, dstRegs,
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
                    IARG_PTR, explicitSrcRegs,
                    IARG_PTR, dstRegs,
                    IARG_END
                );
            }
        }

        // Write memory access
        for(auto i = writtenMemOperands.begin(); i != writtenMemOperands.end(); ++i){
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
                    IARG_PTR, explicitSrcRegs, 
                    IARG_PTR, dstRegs,
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
                    IARG_PTR, explicitSrcRegs, 
                    IARG_PTR, dstRegs,
                    IARG_END
                ); 
            }          
        }
    }

    /*
        If it's a FPU pop instruction, increment the fpu stack index after the register has been read
    */
    if(isFpuPopInstruction(opcode)){
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR) incrementFpuStackIndex,
            IARG_END
        );

        // These 2 instructions decrement the stack top twice
        if(opcode == XED_ICLASS_FUCOMPP || opcode == XED_ICLASS_FCOMPP){
            INS_InsertPredicatedCall(
                ins, 
                IPOINT_BEFORE,
                (AFUNPTR) incrementFpuStackIndex,
                IARG_END
            );
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

    // We take the size of any register, as they have the same size (excluding SIMD extension registers)
    int regSize = REG_Size(REG_STACK_PTR);
    memOverlaps.write("\x00\x00\x00\x00", 4);
    memOverlaps << regSize << ";";
    for(auto iter = imgs_base.begin(); iter != imgs_base.end(); ++iter){
        memOverlaps << iter->first << ";";
        memOverlaps.write(reinterpret_cast<const char*>(&(iter->second)), regSize);
    }
    // End of loaded libraries base addresses
    memOverlaps.write("\x00\x00\x00\x05", 4);
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
                // Do not report instructions coming from the loader's library
                if(ignoreLdInstructions && isLoaderInstruction(v_it->getActualIP()))
                    continue;
                
                memOverlaps.write((v_it->getIsUninitializedRead() ? "\x0a" : "\x0b"), 1);
                tmp = v_it->getIP();
                memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
                tmp = v_it->getActualIP();
                memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
                memOverlaps << v_it->getDisasm() << ";";
                memOverlaps.write((v_it->getType() == AccessType::WRITE ? "\x1a" :"\x1b"), 1);
                memOverlaps << v_it->getSize() << ";";
                memOverlaps.write(v_it->isStackAccess() ? "\x1c" : "\x1d", 1);
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

        if(!containsReadIns(it->first)){
            continue;
        }
        
        set<PartialOverlapAccess> tempSet = PartialOverlapAccess::convertToPartialOverlaps(it->second, true);
        PartialOverlapAccess::addToSet(tempSet, fullOverlaps[it->first]);

        set<PartialOverlapAccess> v;
        unordered_map<MemoryAccess, unordered_set<size_t>, MemoryAccess::NoOrderHasher, MemoryAccess::Comparator> reportedGroups;
        MemoryAccess::NoOrderHasher maHasher;

        // Scan tempSet, and insert in set v the uninitializd read accesses with the write accesses they read from
        // only if the whole group (uninitialized read + write) has not been already inserted yet.
        // NOTE: this is quite similar to what we have done during analysis in |memtrace| when an uninitialized read is found.
        // However, in order to avoid slowing down the analysis itself, we didn't check which write accesses are actually read by
        // the uninitialized read. While that is enough to remove most of the duplicated groups of accesses,
        // it is possible that some are not removed. This way, we are also removing from the set of partial overlaps all those write accesses
        // that are never read by any uninitialized read access.
        for(set<PartialOverlapAccess>::iterator v_it = tempSet.begin(); v_it != tempSet.end(); ++v_it){
            if(v_it->getType() == AccessType::READ && v_it->getIsUninitializedRead() && !v_it->getIsPartialOverlap()){
                set<PartialOverlapAccess> writes;
                
                const MemoryAccess& ma = v_it->getAccess();
                size_t hash = maHasher(ma);
                for(set<PartialOverlapAccess>::iterator writeIt = tempSet.begin(); writeIt != v_it; ++writeIt){
                    if(writeIt->getType() == AccessType::WRITE && isReadByUninitializedRead(writeIt, v_it)){
                        hash = maHasher.lrot(hash, 4) ^ maHasher(writeIt->getAccess());
                        writes.insert(*writeIt);
                    }
                }

                auto overlapGroup = reportedGroups.find(ma);

                if(overlapGroup == reportedGroups.end()){
                    unordered_set<size_t> s;
                    s.insert(hash);
                    reportedGroups[ma] = s;
                }
                else{
                     unordered_set<size_t>&  reportedHashes = overlapGroup->second;

                    // If this is the first time this read access is happening within this context, store it
                    if(reportedHashes.find(hash) == reportedHashes.end()){
                        reportedHashes.insert(hash);
                    }
                    else{
                        continue;
                    }
                }

                v.insert(*v_it);
                for(const PartialOverlapAccess& write : writes){
                    v.insert(write);
                }
            }
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
            if(ignoreLdInstructions && isLoaderInstruction(v_it->getActualIP())){
                continue;
            }
            
            void* uninitializedOverlap = NULL;
            int overlapBeginning = v_it->getAddress() - it->first.getFirst();
            if(overlapBeginning < 0)
                overlapBeginning = 0;

            // NOTE: at this point there are only writes whose bytes are read at least by an uninitialized read access and
            // uninitialized read accesses fully overlapping with the considered set.
            // Moreover, uninitializedOverlap can't be NULL. Every write partially overlaps thi set, and every remained read access is an uninitialized
            // read fully overlapping with the considered set (and so its field |uninitializedInterval| can't be NULL)
            if(v_it->getType() == AccessType::WRITE){
                uninitializedOverlap = getOverlappingWriteInterval(it->first, v_it);
            }
            else{
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
            
            memOverlaps.write(v_it->getIsUninitializedRead() ? "\x0a" : "\x0b", 1);
            tmp = v_it->getIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            tmp = v_it->getActualIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            memOverlaps << v_it->getDisasm() << ";";
            memOverlaps.write((v_it->getType() == AccessType::WRITE ? "\x1a" : "\x1b"), 1);
            memOverlaps << v_it->getSize() << ";";
            memOverlaps.write(v_it->isStackAccess() ? "\x1c" : "\x1d", 1);
            memOverlaps << v_it->getSPOffset() << ";";
            memOverlaps << v_it->getBPOffset() << ";";

            if(v_it->getIsPartialOverlap()){
                memOverlaps.write("\xab\xcd\xef\xff", 4);
            }

            set<std::pair<unsigned, unsigned>> intervals;

            if(v_it->getType() == AccessType::WRITE){
                std::pair<unsigned, unsigned>* pair_ptr = (std::pair<unsigned, unsigned>*) uninitializedOverlap;
                std::pair<unsigned, unsigned> interval = *pair_ptr;
                intervals.insert(interval);
                // Free the pair previously created by a call to getOverlappingWriteInterval
                delete pair_ptr;
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

    // Free every shadow memory
    stack.freeMemory();
    heap.freeMemory();
    for(auto iter = mmapShadows.begin(); iter != mmapShadows.end(); ++iter){
        iter->second.freeMemory();
    }

    // Free all shadow memory copies stored in MemoryAccess objects
    for(auto aiIter = memAccesses.begin(); aiIter != memAccesses.end(); ++aiIter){
        auto& aiSet = aiIter->second;
        for(auto iter = aiSet.begin(); iter != aiSet.end(); ++iter){
            iter->freeMemory();
        }
    }

    // Free all ptrs allocated to pass data structures to analysis functions
    for(auto iter = disasmPtrs.begin(); iter != disasmPtrs.end(); ++iter){
        delete *iter;
    }

    for(auto iter = regsPtrs.begin(); iter != regsPtrs.end(); ++iter){
        delete *iter;
    }

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

    std::string heuristicKnob = KnobHeuristicStatus.Value();
    HeuristicStatus::Status heuristicStatus = HeuristicStatus::fromString(heuristicKnob);
    ignoreLdInstructions = !KnobKeepLoader.Value();

    // If heuristiStatus is LIBS, both the flags are set; if it is ON, only heuristicEnabled is set.
    // If it is OFF (last possible case), nothing is done.
    switch(heuristicStatus){
        case HeuristicStatus::LIBS:
            heuristicLibsOnly = true;
        case HeuristicStatus::ON:
            heuristicEnabled = true;
            break;
        case HeuristicStatus::OFF:
            {}
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
