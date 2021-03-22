
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <cstdarg>

#include "InitializedMemory.h"
#include "SyscallHandler.h"

using std::cerr;
using std::string;
using std::endl;
using std::map;
using std::vector;
using std::set;

/* ================================================================== */
// Global variables 
/* ================================================================== */

std::ostream * out = &cerr;
//std::ofstream routines;
//std::ofstream calls;
std::ofstream memOverlaps;
//std::ofstream stackAddress;

PIN_MUTEX lock;

// FILE* trace;
ADDRINT textStart;
ADDRINT textEnd;
ADDRINT loadOffset;
// TODO: in a multi process or multi threaded application, we need to define
// the following couple of definitions in a per-thread fashion
ADDRINT lastExecutedInstruction;
ADDRINT lastSp;
ADDRINT lastBp;
unsigned long long executedAccesses;

map<AccessIndex, set<MemoryAccess>> fullOverlaps;
map<AccessIndex, set<MemoryAccess>> partialOverlaps;
map<THREADID, ADDRINT> threadInfos;

InitializedMemory* initializedMemory = NULL;
//std::vector<set<AccessIndex>> initializedStack;
std::vector<AccessIndex> retAddrLocationStack;
set<ADDRINT> funcsAddresses;
map<ADDRINT, std::string> funcs;

bool mainCalled = true;
bool entryPointExecuted = false;
ADDRINT mainStartAddr = 0;
ADDRINT mainRetAddr = 0;

int calledFunctions = 0;

ADDRINT syscallIP;

std::ofstream f("i.log");
std::ofstream overlapLog("overlaps.dbg");

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "", "specify file name for MemTrace output");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool generates a memory trace of addresses accessed in read or write mode" << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

UINT32 min(UINT32 x, UINT32 y){
    return x <= y ? x : y;
}

bool isPushInstruction(OPCODE opcode){
    return
        opcode == XED_ICLASS_PUSH ||
        opcode == XED_ICLASS_PUSHA || 
        opcode == XED_ICLASS_PUSHAD || 
        opcode == XED_ICLASS_PUSHF || 
        opcode == XED_ICLASS_PUSHFD || 
        opcode == XED_ICLASS_PUSHFQ;
}

bool isStackAddress(THREADID tid, ADDRINT addr, ADDRINT currentSp, OPCODE* opcode_ptr, AccessType type){
    if(threadInfos.find(tid) == threadInfos.end())
        exit(1);
    OPCODE opcode = *opcode_ptr;
    if(type == AccessType::WRITE && isPushInstruction(opcode)){
        // If it is a push instruction, it surely writes a stack address
        return true;
    }
    /*
    if(addr >= currentSp && addr <= threadInfos[tid])
        stackAddress << "Current sp: 0x" << std::hex << currentSp << "\tStack base: 0x" << threadInfos[tid] << "\tAddr: 0x" << addr << endl;
    */
    return addr >= currentSp && addr <= threadInfos[tid];
}

std::pair<int, int> getUninitializedInterval(AccessIndex& ai){
    return initializedMemory->getUninitializedInterval(ai);
}

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

bool containsUninitializedPartialOverlap(AccessIndex targetAI, set<PartialOverlapAccess>& s){
    for(auto i = s.begin(); i != s.end(); ++i){
        /*
        // If it is a WRITE access, consider it an uninitialized partial overlap only if 
        // there's at least one following access which is an uninitialized read, as it may be 
        // a write access partially overlapping the given targetAI. If it is 
        // not considered uninitialized, it is not added to the partial overlaps
        if(i->getType() == AccessType::WRITE){
            set<PartialOverlapAccess>::iterator following = i;
            ++following;

            while(following != s.end() && !following->getIsUninitializedRead()){
                // If this writes will be overwritten by some other write before it is read,
                // ignore it
                if(
                    following->getType == AccessType::WRITE && 
                    following->getAddress() == i->getAddress() &&
                    following->getSize() == i->getSize()
                ){
                    continue;
                }

                ++following;
            }

            if(following != s.end())
                return true;
            else
                continue;
        }
        */
        // In the first case, it is a read access happening at an address lower than the current set
        // address. It is of no interest here, it will have its own set.
        // Also read accesses which are completely initialized are not interesting.
        if(i->getIsPartialOverlap() || !i->getIsUninitializedRead())
            continue;
        std::pair<int, int> uninitializedInterval = i->getUninitializedInterval();
        int overlapBeginning = i->getAddress() - targetAI.getFirst();
        int overlapEnd = min(i->getAddress() + i->getSize() - 1 - targetAI.getFirst(), targetAI.getSecond() - 1);
        int overlapSize = overlapEnd - overlapBeginning;
        
        if(intervalIntersection(std::pair<int, int>(0, overlapSize), uninitializedInterval) != NULL)
            return true;
        
    }
    return false;
}

bool containsReadIns(set<MemoryAccess, MemoryAccess::ExecutionComparator>& s){
    for(set<MemoryAccess>::iterator i = s.begin(); i != s.end(); ++i){
        if(i->getIsUninitializedRead())
            return true;
    }
    return false;
}

std::pair<unsigned, unsigned>* getOverlappingWriteInterval(const AccessIndex& currentSetAI, set<PartialOverlapAccess>::iterator& v_it){
    int overlapBeginning = v_it->getAddress() - currentSetAI.getFirst();
    if(overlapBeginning < 0)
        overlapBeginning = 0;
    int overlapEnd = min(v_it->getAddress() + v_it->getSize() - 1 - currentSetAI.getFirst(), currentSetAI.getSecond() - 1);
    return new pair<unsigned, unsigned>(overlapBeginning, overlapEnd);
}

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

bool isReadByUninitializedRead(set<PartialOverlapAccess>::iterator& writeAccess, set<PartialOverlapAccess>& s, const AccessIndex& ai){
    ADDRINT writeStart = writeAccess->getAddress();
    UINT32 writeSize = writeAccess->getSize();
    ADDRINT writeEnd = writeStart + writeSize - 1;

    f << "[LOG]: 0x" << std::hex << ai.getFirst() << " - " << std::dec << ai.getSecond() << endl;
    f << "[LOG]: " << std::hex << writeAccess->getDisasm() << " writes " << std::dec << writeSize << " B @ 0x" << std::hex << writeStart << endl;


    set<PartialOverlapAccess>::iterator following = writeAccess;
    ++following;

    unsigned int numOverwrittenBytes = 0;
    bool* overwrittenBytes = new bool[writeSize];

    for(UINT32 i = 0; i < writeSize; ++i){
        overwrittenBytes[i] = false;
    }

    while(following != s.end()){

        if(following == writeAccess)
            abort();

        ADDRINT folStart = following->getAddress();
        ADDRINT folEnd = folStart + following->getSize() - 1;

        // Following access bounds are outside write access bounds
        if(folStart > writeEnd || folEnd < writeStart){
            ++following;
            continue;
        }

        int overlapBeginning = folStart - writeStart;
        if(overlapBeginning < 0)
            overlapBeginning = 0;

        int overlapEnd = min(folEnd - writeStart, writeSize - 1);

        
        if(following->getType() == AccessType::READ && following->getIsUninitializedRead() && folStart >= ai.getFirst()){
            std::pair<unsigned, unsigned>* uninitializedOverlap = getOverlappingUninitializedInterval(ai, following);
            if(uninitializedOverlap == NULL){
                ++following;
                continue;
            }

            f << "[LOG]: " << following->getDisasm() << " reads bytes [" << std::dec << overlapBeginning << " ~ " << overlapEnd << "]" << endl;


            // Check if the read access reads any byte that is not overwritten
            // by any other write access
            for(int i = overlapBeginning; i <= overlapEnd; ++i){
                if(!overwrittenBytes[i]){
                    f << "[LOG]: " << following->getDisasm() << " reads a byte of the write" << endl;
                    // Read access reads not overwritten byte
                    return true;
                }
            }
        }

        if(following->getType() == AccessType::WRITE){

            for(int i = overlapBeginning; i <= overlapEnd; ++i){
                f << "[LOG]: " << following->getDisasm() << " overwrites bytes [" << std::dec << overlapBeginning << " ~ " << overlapEnd << "]" << endl;
                if(!overwrittenBytes[i])
                    ++numOverwrittenBytes;
                overwrittenBytes[i] = true;
            }
            // The write access bytes have been completely overwritten
            // before any read access could read them.
            if(numOverwrittenBytes == writeSize){
                f << "[LOG]: write access completely overwritten" << endl << endl << endl;
                return false;
            }
        }
        
        ++ following;
    }
    f << "[LOG]: write access not completely overwritten, but never read" << endl << endl << endl;
    return false;
}

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

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID detectFunctionStart(ADDRINT ip){
    if(ip < textStart || ip > textEnd){
        return;
    }
    ADDRINT effectiveIp = ip - loadOffset;

    if(ip >= textStart && ip <= textEnd && funcsAddresses.find(effectiveIp) != funcsAddresses.end()){

        // If main has already been detected, it's useless and time-consuming to try and detect it again
        // This way, we avoid a lookup in a map and a string comparison
        if(!mainCalled){
            // Detect if this is 'main' first instruction
            std::string &funcName = funcs[effectiveIp];
            if(!funcName.compare("main")){
                //mainCalled = true;
            }
        }

        //initializedStack.push_back(initializedMemory);
        //initializedMemory.clear();
    }
}

VOID memtrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                VOID* opcode, bool isFirstVisit)
{

    ADDRINT effectiveIp = ip - loadOffset;

    if(isFirstVisit){

        if(ip >= textStart && ip <= textEnd && funcsAddresses.find(effectiveIp) != funcsAddresses.end()){

            // If main has already been detected, it's useless and time-consuming to try and detect it again
            // This way, we avoid a lookup in a map and a string comparison
            if(!mainCalled){
                // Detect if this is 'main' first instruction
                std::string &funcName = funcs[effectiveIp];
                if(!funcName.compare("main")){
                    //mainCalled = true;
                }
            }
        } 
        
    }

    if(!mainCalled)
        return;

    ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);

    REG regBasePtr;
    if(REG_Size(REG_STACK_PTR) == 8){
        regBasePtr = REG_GBP;
    }
    else{
        regBasePtr = REG_EBP;
    }
    ADDRINT bp = PIN_GetContextReg(ctxt, regBasePtr);

    if(ip >= textStart && ip <= textEnd){
        if(!entryPointExecuted){
            entryPointExecuted = true;
            *out << endl << endl << "ENTRY POINT SP: 0x" << std::hex << sp << endl;
            *out << "ENTRY POINT IP: 0x" << std::hex << ip << endl;
            *out << "INSERTING 0x" << std::hex << sp << " - " << std::dec << threadInfos[0] - sp << endl << endl;
            initializedMemory = new InitializedMemory(NULL, threadInfos[0]);
            if(threadInfos[0] - sp > 0){
                AccessIndex ai(sp, (threadInfos[0] - sp));
                initializedMemory->insert(ai);
            }
        }
        // Always refer to an address in the .text section of the executable, never follow libraries addresses
        lastExecutedInstruction = ip - loadOffset;
        // Always refer to the stack created by the executable, ignore accesses to the frames of library functions
        lastSp = sp;

        REG regBasePtr;
        if(REG_Size(REG_STACK_PTR) == 8){
            regBasePtr = REG_GBP;
        }
        else{
            regBasePtr = REG_EBP;
        }

        lastBp = PIN_GetContextReg(ctxt, regBasePtr);
    }
    else{
        if(lastExecutedInstruction == 0){
            return;
        }
    }

    std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);
    OPCODE* opcode_ptr = static_cast<OPCODE*>(opcode);

    // Only keep track of accesses on the stack
    //ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    if(!isStackAddress(tid, addr, sp, opcode_ptr, type)){
        return;
    }

    bool isWrite = type == AccessType::WRITE;
    // fprintf(trace, "0x%lx: %s => %c %u B %s 0x%lx\n", lastExecutedInstruction, ins_disasm->c_str(), isWrite ? 'W' : 'R', size, isWrite ? "to" : "from", addr);
    int spOffset = isPushInstruction(*opcode_ptr) ? 0 : addr - sp;
    int bpOffset = addr - bp;

    MemoryAccess ma(executedAccesses++, lastExecutedInstruction, ip, addr, spOffset, bpOffset, size, type, std::string(*ins_disasm));
    AccessIndex ai(addr, size);

    bool overlapSetAlreadyExists = fullOverlaps.find(ai) != fullOverlaps.end();
    std::pair<int, int> uninitializedInterval;

    if(isWrite){
        initializedMemory->insert(ai);
    }
    else{
        uninitializedInterval = getUninitializedInterval(ai);
        if(uninitializedInterval.first != -1){
            ma.setUninitializedRead();
            ma.setUninitializedInterval(uninitializedInterval);
        }
    }
 
    PIN_MutexLock(&lock);

    if(overlapSetAlreadyExists){
        set<MemoryAccess> &lst = fullOverlaps[ai];
        lst.insert(ma);
    }
    else{
        set<MemoryAccess> v;
        v.insert(ma);
        fullOverlaps[ai] = v;
    }

    PIN_MutexUnlock(&lock);
}

VOID procCallTrace(CONTEXT* ctxt, ADDRINT ip, ADDRINT addr, UINT32 size, ADDRINT targetAddr, ...)
{
    //ADDRINT effectiveIp = ip - loadOffset;

    // This trick is needed as procedure calls write the return address into stack.
    // Normally, the tool set as initialized the cell for the caller, but that's actually used by the callee on return, so,
    // ,n procedure call, insert the written memory area into the initialized memory of the callee.
    // On return, the tool will see the cell containing the return address as initialized, thus removing 
    // "ret" instructions from the results (as they are false positives)
    if(mainCalled){

        ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);

        if(ip >= textStart && ip <= textEnd){
            // Always refer to an address in the .text section of the executable, never follow libraries addresses
            lastExecutedInstruction = ip - loadOffset;
            // Always refer to the stack created by the executable, ignore accesses to the frames of library functions
            lastSp = sp;

            REG regBasePtr;
            if(REG_Size(REG_STACK_PTR) == 8){
                regBasePtr = REG_GBP;
            }
            else{
                regBasePtr = REG_EBP;
            }

            lastBp = PIN_GetContextReg(ctxt, regBasePtr);
        }

        // Insert the address containing function return address as initialized memory
        AccessIndex ai(addr, size);
        retAddrLocationStack.push_back(ai);

        // Push initializedMemory only if the target function is inside the .text section
        // e.g. avoid pushing if library function are called
        //if(targetAddr >= textStart && targetAddr <= textEnd){
            //initializedStack.push_back(initializedMemory);
            initializedMemory = new InitializedMemory(initializedMemory, addr);
            // Avoid clearing initialized memory on procedure calls:
            // every memory cell initialized by a caller, is considered initialized also by the callee.
            //initializedMemory.clear();
        //}

        initializedMemory->insert(ai);

    }

    if(!mainCalled && targetAddr >= textStart && targetAddr <= textEnd){
        if(calledFunctions == 2){
            ++calledFunctions;
            mainCalled = true;
            initializedMemory = new InitializedMemory(initializedMemory, addr);
            *out << "****************" << endl;
            *out << "Main entry point: 0x" << std::hex << targetAddr << endl;
        }
        else{
            ++calledFunctions;
        }
    }
}

VOID retTrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                VOID* opcode, bool isFirstVisit)
{
    if(ip >= textStart && ip <= textEnd){
        // Always refer to an address in the .text section of the executable, never follow libraries addresses
        lastExecutedInstruction = ip - loadOffset;
        // Always refer to the stack created by the executable, ignore accesses to the frames of library functions
        lastSp = PIN_GetContextReg(ctxt, REG_STACK_PTR);

        REG regBasePtr;
        if(REG_Size(REG_STACK_PTR) == 8){
            regBasePtr = REG_GBP;
        }
        else{
            regBasePtr = REG_EBP;
        }

        lastBp = PIN_GetContextReg(ctxt, regBasePtr);
    }

    if(mainCalled && retAddrLocationStack.empty()){
            mainCalled = false;
    }

    // Second check necessary because previous block may have changed 'mainCalled' value
    if(mainCalled){

        //if(ip >= textStart && ip <= textEnd){
            // Restore initializedMemory set as it was before the call of this function
            //initializedMemory.clear();
            //set<AccessIndex> &toRestore = initializedStack.back();
            //initializedMemory.insert(toRestore.begin(), toRestore.end());
            //initializedStack.pop_back();
            initializedMemory = initializedMemory->deleteFrame();
        //}

        AccessIndex &retAddrLocation = retAddrLocationStack.back();
        initializedMemory->insert(retAddrLocation);
        retAddrLocationStack.pop_back();
        
    }

    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, opcode, isFirstVisit);
}


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
        // isFirstVisit always set to false. Probably this bool flag is going to be removed in the next future
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
    //*out << "Entering syscall " << std::dec << sysNum << endl;
    unsigned short argsCount = SyscallHandler::getInstance().getSyscallArgsCount(sysNum);
    vector<ADDRINT> actualArgs;
    for(int i = 0; i < argsCount; ++i){
        ADDRINT arg = PIN_GetSyscallArgument(ctxt, std, i);
        actualArgs.push_back(arg);
    }
    bool lastSyscallReturned = !SyscallHandler::getInstance().init();
    #ifdef DEBUG
        if(!lastSyscallReturned)
            *out << "Current state: " << SyscallHandler::getInstance().getStateName() << "; Reinitializing handler..." << endl << endl;
        *out << endl << "Setting arguments for syscall number " << std::dec << sysNum << endl;
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

        // Saves content of /proc/<PID>/maps to file (For debugging) purposes
        /*
        std::ifstream mapping;
        std::ofstream savedMapping("mapBefore.log");

        INT pid = PIN_GetPid();
        std::stringstream mapPath;
        mapPath << "/proc/" << pid << "/maps";
        mapping.open(mapPath.str().c_str());
        std::string cont((std::istreambuf_iterator<char>(mapping)), (std::istreambuf_iterator<char>()));
        savedMapping << cont << endl;
        */
    }
    *out << IMG_Name(img) << " loaded @ 0x" << std::hex << IMG_LoadOffset(img) << endl;
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    ADDRINT stackBase = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    *out << "Stack base of thread " << tid << " is 0x" << std::hex << stackBase << endl;
    *out << "Stack ptr: 0x" << std::hex << PIN_GetContextReg(ctxt, REG_STACK_PTR) << endl;
    threadInfos.insert(std::pair<THREADID, ADDRINT>(tid, stackBase));
    initializedMemory = new InitializedMemory(NULL, stackBase);
}

VOID OnThreadEnd(THREADID tid, CONTEXT* ctxt, INT32 code, VOID* v){

}

/*VOID Routine(RTN rtn, VOID* v){
    routines << RTN_Name(rtn) << " @ " << std::hex << RTN_Address(rtn) << endl;
}*/

VOID Instruction(INS ins, VOID* v){
    std::stringstream call_str;

    // Prefetch instruction is used to simply trigger memory areas in order to
    // move them to processor's cache. It does not affect program behaviour,
    // but PIN detects it as a memory read operation, thus producing
    // an uninitialized read.
    if(INS_IsPrefetch(ins))
        return;

    
    ADDRINT ins_addr = INS_Address(ins);
    
    if(ins_addr >= textStart && ins_addr <= textEnd){
        // This block can be used to check if it is a dynamic call to a register, to be added in the CFG

        // Used for debug purposes. It creates a file with instructions which are call instructions
        /*
        if(INS_IsCall(ins)){
            call_str << std::hex << "0x" << ins_addr << " is a call instruction";
            if(!INS_IsProcedureCall(ins)){
                call_str << ", but is not a procedure call";
            }
            call_str << endl;
        }

        calls << call_str.str();
        */
        
    }

    bool isProcedureCall = INS_IsProcedureCall(ins);
    bool isRet = INS_IsRet(ins);

    UINT32 memoperands = INS_MemoryOperandCount(ins);

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
            if(INS_MemoryOperandIsWritten(ins, memop) ){
                if(isProcedureCall){
                    IARGLIST args = IARGLIST_Alloc();
                    for(int i = 0; i < 10; ++i){
                        IARGLIST_AddArguments(args, IARG_FUNCARG_CALLSITE_VALUE, i, IARG_END);
                    }
                    INS_InsertPredicatedCall(
                        ins,
                        IPOINT_BEFORE,
                        (AFUNPTR) procCallTrace,
                        IARG_CONTEXT,
                        IARG_INST_PTR,
                        IARG_MEMORYWRITE_EA,
                        IARG_MEMORYWRITE_SIZE,
                        IARG_BRANCH_TARGET_ADDR,
                        IARG_IARGLIST, args,
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

            if(INS_MemoryOperandIsRead(ins, memop)){
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
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{   
    /* 
    fprintf(trace, "\n===============================================\n");
    fprintf(trace, "MEMORY TRACE END\n");
    fprintf(trace, "===============================================\n\n");
    */

    dumpMemTrace();
    std::ofstream t("reverse.log");

    int regSize = REG_Size(REG_STACK_PTR);
    memOverlaps.write("\x00\x00\x00\x00", 4);
    memOverlaps << regSize << ";";

    /*
    The following iterator, and the boolean flag right inside the next for loop scope, are
    used in order to optimize the search of partially overlapping accesses happening at an address lower than the
    address of an access set (denoted as "it" in the loop). Without using these 2 values, we would have
    needed to restart the search from fullOverlaps.begin(), which may require more time.
    */
    std::map<AccessIndex, set<MemoryAccess>>::iterator firstPartiallyOverlappingIterator = fullOverlaps.begin();
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = fullOverlaps.begin(); it != fullOverlaps.end(); ++it){
        bool firstPartiallyOverlappingIteratorUpdated = false;
        // Copy all elements in another set ordered by execution order
        set<MemoryAccess, MemoryAccess::ExecutionComparator> v(it->second.begin(), it->second.end());

        if(containsReadIns(v) && v.size() > 0){

            ADDRINT tmp = it->first.getFirst();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            memOverlaps << it->first.getSecond() << ";";

            /*memOverlaps << "===============================================" << endl;
            memOverlaps << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
            memOverlaps << "===============================================" << endl << endl;*/
            for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); v_it++){
                //int spOffset = v_it->getSPOffset();
                //int bpOffset = v_it->getBPOffset();

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


                /*
                memOverlaps 
                    << (v_it->getIsUninitializedRead() ? "*" : "") 
                    << "0x" << std::hex << v_it->getIP() 
                    << " (0x" << v_it->getActualIP() << ")"
                    << ": " << v_it->getDisasm() << "\t" 
                    << (v_it->getType() == AccessType::WRITE ? "W " : "R ") 
                    << std::dec << v_it->getSize() 
                    << std::hex << " B @ (sp " << (spOffset >= 0 ? "+ " : "- ") 
                    << std::dec << llabs(v_it->getSPOffset()) << ");"
                    << "(bp " << (bpOffset >= 0 ? "+ " : "- ") << llabs(v_it->getBPOffset()) << ")"
                    << endl;
                    */
            }

            // End of full overlap entries
            memOverlaps.write("\x00\x00\x00\x01", 4);

            //memOverlaps << "===============================================" << endl;
            //memOverlaps << "===============================================" << endl << endl << endl << endl << endl;
        }

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
    // End of full overlaps
    memOverlaps.write("\x00\x00\x00\x02", 4);

    std::ofstream overlaps("partialOverlaps.log");
    // Write text file with partial overlaps
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = partialOverlaps.begin(); it != partialOverlaps.end(); ++it){
        set<PartialOverlapAccess> v = PartialOverlapAccess::convertToPartialOverlaps(it->second, true);
        PartialOverlapAccess::addToSet(v, fullOverlaps[it->first]);

        if(v.size() < 1 || !containsUninitializedPartialOverlap(it->first, v)){
            continue;
        }

        ADDRINT tmp = it->first.getFirst();
        
        memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
        memOverlaps << it->first.getSecond() << ";";

        overlapLog << "===============================================" << endl;
        overlapLog << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
        overlapLog << "===============================================" << endl;

        /*
        overlaps << "===============================================" << endl;
        overlaps << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
        overlaps << "===============================================" << endl;
        
        overlaps << "Accessing instructions: " << endl << endl;
        */
        /*for(set<MemoryAccess>::iterator i = fullOverlapsSet.begin(); i != fullOverlapsSet.end(); ++i){
            memOverlaps.write((i->getIsUninitializedRead() ? "\x0a" : "\x0b"), 1);
            tmp = i->getIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            tmp = i->getActualIP();
            memOverlaps.write(reinterpret_cast<const char*>(&tmp), regSize);
            memOverlaps << i->getDisasm() << ";";
            memOverlaps.write((i->getType() == AccessType::WRITE ? "\x1a" : "\x1b"), 1);
            memOverlaps << i->getSize() << ";";
            memOverlaps << i->getSPOffset() << ";";
            memOverlaps << i->getBPOffset() << ";";*/



            /*overlaps 
                << "0x" << std::hex << i->getIP() 
                << " (0x" << i->getActualIP() << ")"
                << ": " << i->getDisasm() << "\t"
                << (i->getType() == AccessType::WRITE ? "W " : "R ") 
                << std::dec << i->getSize() 
                << std::hex << " B @ (sp " << (spOffset >= 0 ? "+ " : "- ")
                << std::dec << llabs(i->getSPOffset()) << ");"
                << "(bp " << (bpOffset >= 0 ? "+ " : "- ") << llabs(i->getBPOffset()) << ")"
                << endl;
        }*/
        //overlaps << "===============================================" << endl;
        
        //overlaps << "Partiallly overlapping instructions: " << endl << endl;

        //memOverlaps.write("\x00\x00\x00\x02", 4);
        
        for(set<PartialOverlapAccess>::iterator v_it = v.begin(); v_it != v.end(); ++v_it){
            //int spOffset = v_it->getSPOffset();
            //int bpOffset = v_it->getBPOffset();

            std::pair<unsigned int, unsigned int>* uninitializedOverlap = NULL;
            int overlapBeginning = v_it->getAddress() - it->first.getFirst();
            if(overlapBeginning < 0)
                overlapBeginning = 0;

            if(v_it->getType() == AccessType::WRITE && isReadByUninitializedRead(v_it, v, it->first))
                uninitializedOverlap = getOverlappingWriteInterval(it->first, v_it);
                    
            if(v_it->getType() == AccessType::READ && v_it->getIsUninitializedRead()){
                if(v_it->getIsPartialOverlap()){
                    uninitializedOverlap = getOverlappingUninitializedInterval(it->first, v_it);
                }
                else{
                    // It is a full overlap, but it is an uninitialized read
                    uninitializedOverlap = new std::pair<unsigned, unsigned>(v_it->getUninitializedInterval());
                }

                // The overlap beginning may not be the start of the uninitialized access itself.
                if(uninitializedOverlap != NULL){
                    uninitializedOverlap->first += overlapBeginning;
                    uninitializedOverlap->second += overlapBeginning;
                }
            }


            if(v_it->getIsPartialOverlap())
                overlapLog << "=> ";
            else
                overlapLog << "   ";

            if(v_it->getIsUninitializedRead())
                overlapLog << "*";

            overlapLog    
                << "0x" << std::hex << v_it->getIP() 
                << " (0x" << v_it->getActualIP() << ")"
                << ": " << v_it->getDisasm() << "\t" 
                << (v_it->getType() == AccessType::WRITE ? "W " : "R ") << std::dec << v_it->getSize() << " B "
                << "@ 0x" << std::hex << v_it->getAddress() << "; (sp " << (v_it->getSPOffset() >= 0 ? "+ " : "- ") << std::dec << llabs(v_it->getSPOffset()) << "); "
                << "(bp " << (v_it->getBPOffset() >= 0 ? "+ " : "- ") << llabs(v_it->getBPOffset()) << ") ";
            if(uninitializedOverlap != NULL)
                overlapLog << "bytes [" << std::dec << uninitializedOverlap->first << " ~ " << uninitializedOverlap->second << "]";
            else
                overlapLog << " {NULL interval} ";
            overlapLog << endl;



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
            
            /*
            overlaps    
                << "0x" << std::hex << v_it->getIP() 
                << " (0x" << v_it->getActualIP() << ")"
                << ": " << v_it->getDisasm() << "\t" 
                << (v_it->getType() == AccessType::WRITE ? "W " : "R ")
                << "@ (sp " << (spOffset >= 0 ? "+ " : "- ") << std::dec << llabs(v_it->getSPOffset()) << "); "
                << "(bp " << (bpOffset >= 0 ? "+ " : "- ") << llabs(v_it->getBPOffset()) << ") "
                << "bytes [" << std::dec << uninitializedOverlap->first << " ~ " << uninitializedOverlap->second << "]" << endl;
            */
        }

        memOverlaps.write("\x00\x00\x00\x03", 4);

        overlapLog << "===============================================" << endl;
        overlapLog << "===============================================" << endl << endl << endl << endl << endl;
        
    }

    memOverlaps.write("\x00\x00\x00\x04", 4);

    
    // fclose(trace);
    // routines.close();
    
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

    PIN_MutexInit(&lock);
    
    string filename = KnobOutputFile.Value();

    if (filename.empty()){
        filename = "memtrace.log";
    }

    // trace = fopen(filename.c_str(), "w");
    // routines.open("routines.log");
    //calls.open("calls.log");
    memOverlaps.open("overlaps.bin", std::ios_base::binary);
    //stackAddress.open("stackAddress.log");
    /*
    std::ifstream functions("funcs.lst");

    ADDRINT addr;
    std::string addr_str;
    std::string name;
    while(functions >> addr_str >> name){
        addr_str = addr_str.substr(2);
        addr = strtoul(addr_str.c_str(), NULL, 16);
        funcsAddresses.insert(addr);
        if(!name.compare("main") || !name.compare("dbg.main")){
            mainStartAddr = addr;
            name.assign("main");
        }
        funcs[addr] = name;
    }

    functions.close();
    functions.open("main_ret.addr");
    functions >> addr_str;
    addr_str = addr_str.substr(2);
    addr = strtoul(addr_str.c_str(), NULL, 16);
    mainRetAddr = addr;

    functions.close();

    for(set<ADDRINT>::iterator i = funcsAddresses.begin(); i != funcsAddresses.end(); ++i){
        *out << "Function detected @ 0x" << std::hex << *i << endl;
    }

    *out << "Main start address: 0x" << std::hex << mainStartAddr << endl;
    *out << "Main return address: 0x" << std::hex << mainRetAddr << endl;
    */
    
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    //RTN_AddInstrumentFunction(Routine, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Manage syscalls
    PIN_AddSyscallEntryFunction(onSyscallEntry, NULL);
    PIN_AddSyscallExitFunction(onSyscallExit, NULL);
    
    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MemTrace" << endl;
    cerr << "See file " << filename << " for analysis results" << endl;
    cerr <<  "===============================================" << endl;

    /*
    fprintf(trace, "===============================================\n");
    fprintf(trace, "MEMORY TRACE START\n");
    fprintf(trace, "===============================================\n\n");
    fprintf(trace, "<Program Counter>: <Instruction_Disasm> => R/W <Address>\n\n");
    */

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */