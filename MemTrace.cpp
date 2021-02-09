
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <cstdarg>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>

#include "MemoryAccess.h"
#include "AccessIndex.h"

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

map<AccessIndex, set<MemoryAccess>> fullOverlaps;
map<AccessIndex, set<MemoryAccess>> partialOverlaps;
map<THREADID, ADDRINT> threadInfos;

set<AccessIndex> initializedMemory;
std::vector<set<AccessIndex>> initializedStack;
std::vector<AccessIndex> retAddrLocationStack;
set<ADDRINT> funcsAddresses;
map<ADDRINT, std::string> funcs;

bool mainCalled = false;
ADDRINT mainStartAddr = 0;
ADDRINT mainRetAddr = 0;

int sockfd, serversock;
struct sockaddr_in address;


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

void send_address(unsigned long ip){
    unsigned int digits = 20;
    char ip_str[20];
    if(ip != 0){
        snprintf(ip_str, digits, "0x%lx", ip);
        write(sockfd, ip_str, digits);
    }
    else{
        write(sockfd, "0", 2);
    }
}

int receiveArgCount(){
    char counter_str[50];
    read(sockfd, counter_str, 50);
    return atoi(counter_str);
}

UINT32 min(UINT32 x, UINT32 y){
    return x <= y ? x : y;
}

bool isStackAddress(THREADID tid, ADDRINT addr, ADDRINT currentSp, std::string* disasm, AccessType type){
    if(threadInfos.find(tid) == threadInfos.end())
        exit(1);
    if(type == AccessType::WRITE && disasm->find("push") != disasm->npos){
        // If it is a push instruction, it surely writes a stack address
        return true;
    }
    /*
    if(addr >= currentSp && addr <= threadInfos[tid])
        stackAddress << "Current sp: 0x" << std::hex << currentSp << "\tStack base: 0x" << threadInfos[tid] << "\tAddr: 0x" << addr << endl;
    */
    return addr >= currentSp && addr <= threadInfos[tid];
}

bool readsInitializedMemory(AccessIndex& ai){
    if(initializedMemory.find(ai) != initializedMemory.end()){
        return true;
    }

    std::pair<unsigned int, unsigned int> uninitializedBytes(0, ai.getSecond());
    set<AccessIndex>::iterator iter = initializedMemory.begin();

    ADDRINT targetStart = ai.getFirst();
    ADDRINT targetEnd = targetStart + ai.getSecond() - 1;

    while(iter != initializedMemory.end()){
        ADDRINT currentStart = iter->getFirst();
        ADDRINT currentEnd = currentStart + iter->getSecond() - 1;
        // If will remain uninitialized bytes at the beginning, between iter start and target start
        if(currentStart > targetStart + uninitializedBytes.first){
            return false;
        }
        // If there are no more uninitialized bytes
        if(targetEnd <= currentEnd){
            return true;
        }

        if(currentEnd >= targetStart){
            uninitializedBytes.first += iter->getSecond() - (targetStart + uninitializedBytes.first - currentStart);
        }
        ++iter;   
    }
    return false;
}

bool containsReadIns(set<MemoryAccess> s){
    for(set<MemoryAccess>::iterator i = s.begin(); i != s.end(); ++i){
        if(i->getIsUninitializedRead())
            return true;
    }
    return false;
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
                mainCalled = true;
            }
        }

        //initializedStack.push_back(initializedMemory);
        //initializedMemory.clear();
    }
}

VOID memtrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                bool isFirstVisit)
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
                    mainCalled = true;
                }
            }
        } 
        
    }

    if(!mainCalled)
        return;

    if(ip >= textStart && ip <= textEnd){
        // Always refer to an address in the .text section of the executable, never follow libraries addresses
        lastExecutedInstruction = ip - loadOffset;
        // Always refer to the stack created by the executable, ignore accesses to the frames of library functions
        lastSp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    }
    else{
        if(lastExecutedInstruction == 0){
            return;
        }
    }

    std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);

    // Only keep track of accesses on the stack
    //ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    if(!isStackAddress(tid, addr, lastSp, ins_disasm, type)){
        return;
    }

    bool isWrite = type == AccessType::WRITE;
    // fprintf(trace, "0x%lx: %s => %c %u B %s 0x%lx\n", lastExecutedInstruction, ins_disasm->c_str(), isWrite ? 'W' : 'R', size, isWrite ? "to" : "from", addr);

    MemoryAccess ma(lastExecutedInstruction, addr, size, type, std::string(*ins_disasm));
    AccessIndex ai(addr, size);

    bool overlapSetAlreadyExists = fullOverlaps.find(ai) != fullOverlaps.end();
    set<MemoryAccess> &lst = fullOverlaps[ai];
    std::pair<int, int> uninitializedInterval;

    if(isWrite){
        initializedMemory.insert(ai);
    }
    else if(!readsInitializedMemory(ai)){
            ma.setUninitializedRead();
    }
    
 
    PIN_MutexLock(&lock);

    if(overlapSetAlreadyExists){
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
        }

        // Insert the address containing function return address as initialized memory
        AccessIndex ai(addr, size);
        retAddrLocationStack.push_back(ai);

        /* Arguments handling (OLD)
        int args_count;
        if(targetAddr >= textStart && targetAddr <= textEnd){
            send_address(targetAddr);
            args_count = receiveArgCount();
        }
        else{
            args_count = 10;
        }

        std::va_list args;
        va_start(args, targetAddr);
        set<AccessIndex> toPropagate;
        int stackArgumentIndex = 0;
        for(int i = 0; i < args_count; ++i){
            ADDRINT argument = va_arg(args, ADDRINT);
            ADDRINT* arg_ptr = (ADDRINT*) sp;
            ADDRINT arg_val;
            arg_ptr += stackArgumentIndex;
            PIN_SafeCopy(&arg_val, arg_ptr, sizeof(ADDRINT));
            if(arg_val == argument){
                ++stackArgumentIndex;
                AccessIndex argPtrAI((ADDRINT) arg_ptr, sizeof(ADDRINT));
                //toPropagate.insert(argPtrAI);
            }
            

            *out << "0x" << std::hex << effectiveIp << ": New argument ";
            if(arg_val == argument){
                *out << "stored @ 0x" << std::hex << arg_ptr << " ";
            }
            *out << "propagated: 0x" << argument << endl;
            for(set<AccessIndex>::iterator i = initializedMemory.begin(); i != initializedMemory.end(); ++i){
                if(i->getFirst() == argument){
                    toPropagate.insert(AccessIndex(argument, i->getSecond()));
                }
            }
        }
        va_end(args);
        */

        // Push initializedMemory only if the target function is inside the .text section
        // e.g. avoid pushing if library function are called
        if(targetAddr >= textStart && targetAddr <= textEnd){
            initializedStack.push_back(initializedMemory);
            // Avoid clearing initialized memory on procedure calls:
            // every memory cell initialized by a caller, is considered initialized also by the callee.
            //initializedMemory.clear();
        }

        //initializedMemory.insert(toPropagate.begin(), toPropagate.end());

    }
}

VOID retTrace(  THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr,
                bool isFirstVisit)
{
    if(ip >= textStart && ip <= textEnd){
        // Always refer to an address in the .text section of the executable, never follow libraries addresses
        lastExecutedInstruction = ip - loadOffset;
        // Always refer to the stack created by the executable, ignore accesses to the frames of library functions
        lastSp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    }

    if(mainCalled && retAddrLocationStack.empty()){
            mainCalled = false;
    }

    // Second check necessary because previous block may have changed 'mainCalled' value
    if(mainCalled){

        if(ip >= textStart && ip <= textEnd){
            // Restore initializedMemory set as it was before the call of this function
            initializedMemory.clear();
            set<AccessIndex> &toRestore = initializedStack.back();
            initializedMemory.insert(toRestore.begin(), toRestore.end());
            initializedStack.pop_back();
        }

        AccessIndex &retAddrLocation = retAddrLocationStack.back();
        initializedMemory.insert(retAddrLocation);
        retAddrLocationStack.pop_back();
        
    }

    memtrace(tid, ctxt, type, ip, addr, size, disasm_ptr, isFirstVisit);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Image(IMG img, VOID* v){
    if(IMG_IsMainExecutable(img)){
        *out << "Main executable: " << IMG_Name(img) << endl;
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
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    ADDRINT stackBase = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    *out << "Stack base of thread " << tid << " is 0x" << std::hex << stackBase << endl;
    threadInfos.insert(std::pair<THREADID, ADDRINT>(tid, stackBase));
}

VOID OnThreadEnd(THREADID tid, CONTEXT* ctxt, INT32 code, VOID* v){

}

/*VOID Routine(RTN rtn, VOID* v){
    routines << RTN_Name(rtn) << " @ " << std::hex << RTN_Address(rtn) << endl;
}*/

VOID Instruction(INS ins, VOID* v){
    std::stringstream call_str;

    
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
        INS_InsertCall(
        ins, 
        IPOINT_BEFORE, 
        (AFUNPTR) detectFunctionStart, 
        IARG_INST_PTR, 
        IARG_END);
    }
    else{
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
                        IARG_PTR, new std::string(INS_Disassemble(ins)), 
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
                        IARG_PTR, new std::string(INS_Disassemble(ins)),
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
                        IARG_PTR, new std::string(INS_Disassemble(ins)),
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

    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = fullOverlaps.begin(); it != fullOverlaps.end(); ++it){
        // Write text file with full overlaps
        set<MemoryAccess> v = it->second;
        if(containsReadIns(v)){
            if(v.size() > 1){
                memOverlaps << "===============================================" << endl;
                memOverlaps << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
                memOverlaps << "===============================================" << endl << endl;
                for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); v_it++){
                    memOverlaps << (v_it->getIsUninitializedRead() ? "*" : "") << "0x" << std::hex << v_it->getIP() << ": " << v_it->getDisasm() << "\t" << (v_it->getType() == AccessType::WRITE ? "W " : "R ") << std::dec << v_it->getSize() << std::hex << " B @ 0x" << v_it->getAddress() << endl;
                }
                memOverlaps << "===============================================" << endl;
                memOverlaps << "===============================================" << endl << endl << endl << endl << endl;
            }

            // Fill the partial overlaps map
            ADDRINT lastAccessedByte = it->first.getFirst() + it->first.getSecond() - 1;
            std::map<AccessIndex, set<MemoryAccess>>::iterator partialOverlapIterator = fullOverlaps.find(it->first);
            ++partialOverlapIterator;
            ADDRINT accessedAddress = partialOverlapIterator->first.getFirst();
            while(partialOverlapIterator != fullOverlaps.end() && accessedAddress <= lastAccessedByte){
                //if(containsReadIns(partialOverlapIterator->second)){
                    if(partialOverlaps.find(it->first) != partialOverlaps.end()){
                        set<MemoryAccess> &vect = partialOverlaps[it->first];
                        vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());
                    }
                    else{
                        set<MemoryAccess> vect;
                        vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());
                        partialOverlaps[it->first] = vect;
                    }
                //}

                ++partialOverlapIterator;
                accessedAddress = partialOverlapIterator->first.getFirst();
            }
        }
    }

    std::ofstream overlaps("partialOverlaps.log");
    // Write text file with partial overlaps
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = partialOverlaps.begin(); it != partialOverlaps.end(); ++it){
        set<MemoryAccess> v = it->second;
        if(v.size() < 1 || !containsReadIns(v)){ 
            continue;
        }
        overlaps << "===============================================" << endl;
        overlaps << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
        overlaps << "===============================================" << endl;

        for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); ++v_it){
            ADDRINT overlapBeginning = v_it->getAddress() - it->first.getFirst();
            ADDRINT overlapEnd = min(overlapBeginning + v_it->getSize() - 1, it->first.getSecond() - 1);

            if(!v_it->getIsUninitializedRead())
                continue;

            overlaps    << "0x" << std::hex << v_it->getIP() << ": " << v_it->getDisasm() << "\t" 
                        << (v_it->getType() == AccessType::WRITE ? "W " : "R ")
                        << "bytes [" << std::dec << overlapBeginning << " ~ " << overlapEnd << "]" << endl;
        }
        overlaps << "===============================================" << endl;
        overlaps << "===============================================" << endl << endl << endl << endl << endl;
    }

    
    // fclose(trace);
    // routines.close();
    send_address(0);
    
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
    memOverlaps.open("overlaps.log");
    //stackAddress.open("stackAddress.log");
    std::ifstream functions("funcs.lst");

    // Prepare socket connection
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("Cannot create socket\n");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(8123);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if(connect(sockfd, (struct sockaddr*) &address, sizeof(address)) < 0){
        perror("Connection failed");
        return 2;
    }

    *out << "Connection successfully established" << endl;

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
    
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    //RTN_AddInstrumentFunction(Routine, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    
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
