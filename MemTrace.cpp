
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
#include <cstring>

#include <ctime>

#include "ShadowMemory.h"
#include "AccessIndex.h"
#include "MemoryAccess.h"
#include "SyscallHandler.h"

using std::cerr;
using std::string;
using std::endl;
using std::map;
using std::tr1::unordered_set;
using std::vector;
using std::set;

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

map<AccessIndex, set<MemoryAccess>> fullOverlaps;
// The following set is needed in order to optimize queries about sets containing
// uninitialized read accesses. If a set contains at least 1 uninitialized read,
// the correspondin AccessIndex object is inserted in the set (implemented as an hash table).
unordered_set<AccessIndex, AccessIndex::AIHasher> containsUninitializedRead;
map<AccessIndex, set<MemoryAccess>> partialOverlaps;

ADDRINT libc_base;

//InitializedMemory* initializedMemory = NULL;
std::vector<AccessIndex> retAddrLocationStack;

unsigned long SHADOW_ALLOCATION;
std::vector<uint8_t*> shadow;
uint8_t* highestShadowAddr;

// NOTE: this map is useful only in case of a multi-process/multi-threaded application.
// However, the pintool currently supports only single threaded applications; this map has been
// defined as a future extension of the tool to support multi-threaded applications.
map<THREADID, ADDRINT> threadInfos;

bool entryPointExecuted = false;

// Global variable required as the syscall IP is only retrievable at syscall
// entry point, but it is required at syscall exit point to be added to the
// application's memory accesses.
ADDRINT syscallIP;

#ifdef DEBUG
    std::ofstream isReadLogger("isReadLog.log");
    std::ofstream analysisProfiling("MemTrace.profile");
    std::ofstream applicationTiming("appTiming.profile");
#endif

/* ===================================================================== */
// Command line switches
/* ===================================================================== */


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

bool isCallInstruction(OPCODE opcode){
    return
        opcode == XED_ICLASS_CALL_FAR ||
        opcode == XED_ICLASS_CALL_NEAR;
}

bool isStackAddress(THREADID tid, ADDRINT addr, ADDRINT currentSp, OPCODE* opcode_ptr, AccessType type){
    // If the thread ID is not found, there's something wrong.
    if(threadInfos.find(tid) == threadInfos.end()){
        *out << "Thread id not found" << endl;
        exit(1);
    }

    OPCODE opcode = *opcode_ptr;

    // NOTE: check on the access type in the predicate is required because a push/call instruction may also 
    // read from a memory area, which can be from any memory section (e.g. stack, heap, global variables...)
    if(type == AccessType::WRITE){
        if(isPushInstruction(opcode)){
            // If it is a push instruction, it surely writes a stack address
            return true;
        }

        if(isCallInstruction(opcode))
            return true;
    }

    return addr >= currentSp && addr <= threadInfos[tid];
}

// Function to retrieve the bytes of the given Access Index object which are considered not initialized.
// The interval contains the offset of the bytes from the beginning of the access represented by
// the given AccessIndex object.
// For instance, if a write already initialized bytes [0x0, 0xa] of memory and the given access index accesses 
// [0x0, 0xf], the returned pair would be (11, 15).

// NOTE: this is not a precise result. If the access represented by |ai| is fully initialized except 1 byte,
// the returned pair will report as uninitialized the whole interval from the uninitialized byte to the end of the
// access.
// This has been done because:
//  [*] It shouldn't be very frequent that an access to an area of the stack has an uninitialized "hole"
//  [*] This function is quite expensive, as it essentially scans the recorded memory accesses looking for 
//      write accesses who wrote a certain byte. In order to try and reduce its cost, the function returns as soon
//      as it detects an uninitialized byte.


// Utility function to compute the intersection of 2 intervals.
std::pair<unsigned int, unsigned int>* intervalIntersection(std::pair<unsigned int, unsigned int> int1, std::pair<unsigned int, unsigned int> int2){
    if(int1.first <= int2.first){
        if(int1.second < int2.first)
            return NULL;
        return new std::pair<unsigned int, unsigned int>(int2.first, min(int1.second, int2.second));
    }
    else{
        if(int2.second < int1.first)
            return NULL;
        return new std::pair<unsigned int, unsigned int>(int1.first, min(int1.second, int2.second));
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

// Similar to getUninitializedInterval, but the returned pair contains the bounds of the interval of bytes written
// by the access pointed to by |v_it| which overlap with the access represented by |currentSetAI|
std::pair<unsigned, unsigned>* getOverlappingWriteInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it){
    int overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    if(overlapBeginning < 0)
        overlapBeginning = 0;
    int overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    return new pair<unsigned, unsigned>(overlapBeginning, overlapEnd);
}

// Similar to getUninitializedInterval, but the returned pair contains the bounds of the interval of bytes
// read by the access pointed to by |v_it| which are also considered not initialized and overlap with
// the access represented by |currentSetAI|.

std::pair<unsigned, unsigned>* getOverlappingUninitializedInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it){
    // If the read access happens at an address lower that the set address, it is of no interest
    if(v_it->getAddress() < currentSetAI.getFirst())
        return NULL;
    
    // Return the part of the partial overlap pointed to by v_it that is uninitialized w.r.t. currentSetAI.
    // If the whole partial overlap is initialized, return NULL
    ADDRINT overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    ADDRINT overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    ADDRINT overlapSize = overlapEnd - overlapBeginning; 
    return intervalIntersection(std::pair<unsigned int, unsigned int>(0, overlapSize), v_it->getUninitializedInterval());
}

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
            std::pair<unsigned, unsigned>* uninitializedOverlap = getOverlappingUninitializedInterval(ai, following);
            // If the uninitialized read access does not overlap |ai| access, it can overlap |writeAccess|, but it is 
            // of no interest now.
            if(uninitializedOverlap == NULL){
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

// Utility function that dumps all the memory accesses recorded during application's execution
// to a file named memtrace.log
void dumpMemTrace(){
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

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID memtrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                VOID* opcode, bool isFirstVisit)
{
    #ifdef DEBUG
        print_profile(applicationTiming, "Tracing new memory access");
    #endif

    ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    OPCODE* opcode_ptr = static_cast<OPCODE*>(opcode);

    // Only keep track of accesses on the stack, so if it is an access to any other memory
    // section, return immediately.
    if(!isStackAddress(tid, addr, sp, opcode_ptr, type)){
        return;
    }

    // This is an application instruction
    if(ip >= textStart && ip <= textEnd){
        if(!entryPointExecuted){
            entryPointExecuted = true;
            highestShadowAddr = getShadowAddr(addr);
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

    bool isWrite = type == AccessType::WRITE;
    // If it is a writing push instruction, it increments sp and writes it, so spOffset is 0
    int spOffset = isPushInstruction(*opcode_ptr) ? 0 : addr - sp;
    int bpOffset = addr - bp;

    MemoryAccess ma(executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, std::string(*ins_disasm));
    AccessIndex ai(addr, size);

    if(isWrite){
        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing write access");
        #endif
        //initializedMemory->insert(ai);
        set_as_initialized(addr, size);
    }
    else{
        #ifdef DEBUG
            print_profile(applicationTiming, "\tTracing read access");
        #endif
        std::pair<int, int> uninitializedInterval = getUninitializedInterval(addr, size);
        #ifdef DEBUG
            print_profile(applicationTiming, "\t\tFinished retrieving uninitialized overlap");
        #endif
        if(uninitializedInterval.first != -1){
            ma.setUninitializedRead();
            ma.setUninitializedInterval(uninitializedInterval);
            containsUninitializedRead.insert(ai);
        }
    }
    
    #ifdef DEBUG
        print_profile(applicationTiming, "Inserting access to map");
    #endif

    const auto& overlapSet = fullOverlaps.find(ai);
    if(overlapSet != fullOverlaps.end()){
        overlapSet->second.insert(ma);
    }
    else{
        set<MemoryAccess> v;
        v.insert(ma);
        fullOverlaps[ai] = v;
    }
}

// Procedure call instruction pushes the return address on the stack. In order to insert it as initialized memory
// for the callee frame, we need to first initialize a new frame and then insert the write access into its context.
VOID procCallTrace( THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                    VOID* opcode, bool isFirstVisit)
{
    if(!entryPointExecuted)
        return;
    /*
    if(ip >= textStart && ip <= textEnd){
        // Compute the last executed application ip. This is useless when application code is executed, but may be useful
        // to track where a library function has been called or jumped to
        lastExecutedInstruction = ip - loadOffset;
    }*/

    AccessIndex ai(addr, size);
    //retAddrLocationStack.push_back(ai);

    // Initialized a new frame and insert the saved return address in its context
    //initializedMemory = new InitializedMemory(initializedMemory, addr);
    //initializedMemory->insert(ai);


    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, isFirstVisit);
}

VOID retTrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                VOID* opcode, bool isFirstVisit)
{
    if(!entryPointExecuted)
        return;
    // If there are no other frames to return to, we are returning from the entry point, so there's anything
    // else to do.

    // NOTE: usually the entry point performs jumps to other functions.
    // E.g. on Linux, it's __libc_start_main who calls function main, and __libc_start_main is the first function
    // that can be seen in the call stack by using a debugger. This means that __libc_start_main has been executed by
    // a sequence of jumps from the entry point (_start).
    // However, if __libc_start_main returns, there's nothing else to do.
    /*if(retAddrLocationStack.empty())
        return;*/

    /*
    if(ip >= textStart && ip <= textEnd){
        // Compute the last executed application ip. This is useless when application code is executed, but may be useful
        // to track where a library function has been called or jumped to
        lastExecutedInstruction = ip - loadOffset;
    }*/
        
    // If the input triggers an application vulnerability, it is possible that the return instruction reads an uninitialized
    // memory area. Call memtrace to analyze the read access.
    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, isFirstVisit);

    //initializedMemory = initializedMemory->deleteFrame();

    reset(addr);
    
    //retAddrLocationStack.pop_back();
}


// Given a set of SyscallMemAccess objects, generated by the SyscallHandler on system calls,
// add them to the recorded memory accesses
void addSyscallToAccesses(THREADID tid, CONTEXT* ctxt, set<SyscallMemAccess>& v){
    #ifdef DEBUG
        string* disasm = new string("syscall");
    #else
        string* disasm = new string();
    #endif
    OPCODE* opcode = (OPCODE*) malloc(sizeof(OPCODE));
    // Actually, it could be another kind of syscall (e.g. int 0x80)
    // but opcode is simply used to be compared to the push opcode, so 
    // does not make any difference
    *opcode = XED_ICLASS_SYSCALL_AMD;
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
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    ADDRINT stackBase = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    threadInfos.insert(std::pair<THREADID, ADDRINT>(tid, stackBase));
    // Initialize first function frame
    //initializedMemory = new InitializedMemory(NULL, stackBase);

    // Insert current stack pointer entry as initialized.
    // This should be pushed by the loader, and the entry point reads it (through pop).
    // Set it as initialized to avoid a false positive to be reported.
    // NOTE: actually, if we are on a 32 bit architecture, the stack entry is 4 bytes long,
    // but it is not important here, as the whole portion from the current stack pointer to the
    // highest address of the stack address range can be considered initialized.
    // However, entries stored at an higher address than the last pushed value are never read after the
    // entry point is started, so they won't generate any false positive.
    AccessIndex ai(stackBase, 8);
    //initializedMemory->insert(ai);
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
            std::string* disassembly = new std::string("");
        #endif

        OPCODE* opcode = (OPCODE*) malloc(sizeof(OPCODE));
        *opcode = INS_Opcode(ins);

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
                        IARG_PTR, opcode, 
                        IARG_BOOL, memop == 0,
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
                        IARG_PTR, opcode, 
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
                        IARG_PTR, opcode,
                        IARG_BOOL, memop == 0,
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
                        IARG_PTR, opcode,
                        IARG_BOOL, memop == 0,
                        IARG_END
                    );
                }
            }
        }
    }
}   

/*!
 * Generate overlap reports.
 * This function is called when the application exits.
 */
VOID Fini(INT32 code, VOID *v)
{   
    std::ofstream memOverlaps("overlaps.bin", std::ios_base::binary);
    #ifdef DEBUG
        print_profile(applicationTiming, "Application exited");
        std::ofstream partialOverlapsLog("partialOverlaps.dbg");
    #endif

    dumpMemTrace();

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
                    std::pair<int, int> interval = v_it->getUninitializedInterval();
                    memOverlaps << interval.first << ";";
                    memOverlaps << interval.second << ";";
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

            std::pair<unsigned int, unsigned int>* uninitializedOverlap = NULL;
            int overlapBeginning = v_it->getAddress() - it->first.getFirst();
            if(overlapBeginning < 0)
                overlapBeginning = 0;

            if(v_it->getType() == AccessType::WRITE && isReadByUninitializedRead(v_it, v, it->first))
                uninitializedOverlap = getOverlappingWriteInterval(it->first, v_it);
                    
            // If it is a READ access and it is uninitialized and execution reaches this branch,
            // it surely is an uninitialized read fully overlapping with the considered set
            if(v_it->getType() == AccessType::READ && v_it->getIsUninitializedRead()){
                uninitializedOverlap = new std::pair<unsigned, unsigned>(v_it->getUninitializedInterval());
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
                    partialOverlapsLog << "bytes [" << std::dec << uninitializedOverlap->first << " ~ " << uninitializedOverlap->second << "]";
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

            if(uninitializedOverlap != NULL){
                // Entry has uninitialized interval
                memOverlaps.write("\xab\xcd\xef\xff", 4);
                if(v_it->getIsPartialOverlap()){
                    // Entry is a partial overlap
                    memOverlaps.write("\xab\xcd\xef\xff", 4);
                    if((unsigned)overlapBeginning != uninitializedOverlap->first){
                        // overlapBeginning is smaller than uninitializedOverlap.
                        // This may happen with uninitialized partially overlapping read accesses.
                        // If that is the case, write 2 intervals, the first is the initialized portion,
                        // the second is the uninitialized one.
                        memOverlaps.write("\xab\xcd\xef\xff", 4);
                        memOverlaps << overlapBeginning << ";";
                        memOverlaps << uninitializedOverlap->first - 1 << ";";
                    }
                }
                memOverlaps << uninitializedOverlap->first << ";";
                memOverlaps << uninitializedOverlap->second << ";";
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
    //PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    shadow.reserve(25);
    long pagesize = sysconf(_SC_PAGESIZE);
    SHADOW_ALLOCATION = pagesize;
    for(int i = 0; i < 5; ++i){
        uint8_t* newMap = (uint8_t*) mmap(NULL, SHADOW_ALLOCATION, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(newMap == (void*) -1){
            printf("mmap failed: %s\n", strerror(errno));
            exit(1);
        }
        shadow.push_back(newMap);
    }
    *shadow[0] = 0xff;
    
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

    // Trigger unordered_map rehash in order to reduce probability
    // of rehashing during analysis, as it is an expensive operation
    // NOTE: it is not impossible that rehash is however triggered during the analysis,
    // but doing it now should prevent to do that very early
    containsUninitializedRead.rehash(1 << 20);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}