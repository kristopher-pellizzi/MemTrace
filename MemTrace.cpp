
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
std::ofstream routines;
std::ofstream calls;
std::ofstream memOverlaps;
std::ofstream stackAddress;

PIN_MUTEX lock;

FILE* trace;
ADDRINT textStart;
ADDRINT textEnd;
ADDRINT loadOffset;
// TODO: in a multi process or multi threaded application, we need to define
// the following couple of definitions in a per-thread fashion
ADDRINT lastExecutedInstruction;
ADDRINT lastSp;

//map<ADDRINT, vector<MemoryAccess>> accesses;
map<AccessIndex, set<MemoryAccess>> fullOverlaps;
map<AccessIndex, set<MemoryAccess>> partialOverlaps;
map<THREADID, ADDRINT> threadInfos;


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

bool isStackAddress(THREADID tid, ADDRINT addr, ADDRINT currentSp){
    if(threadInfos.find(tid) == threadInfos.end())
        exit(1);
    if(addr >= currentSp && addr <= threadInfos[tid])
        stackAddress << "Current sp: 0x" << std::hex << currentSp << "\tStack base: 0x" << threadInfos[tid] << "\tAddr: 0x" << addr << endl;
    return addr >= currentSp && addr <= threadInfos[tid];
}

UINT32 min(UINT32 x, UINT32 y){
    return x <= y ? x : y;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID memtrace(THREADID tid, CONTEXT* ctxt, AccessType type, ADDRINT ip, ADDRINT addr, UINT32 size, VOID* disasm_ptr){
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

    // Only keep track of accesses on the stack
    //ADDRINT sp = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    if(!isStackAddress(tid, addr, lastSp)){
        return;
    }

    bool isWrite = type == AccessType::WRITE;
    std::string* ins_disasm = static_cast<std::string*>(disasm_ptr);
    fprintf(trace, "0x%lx: %s => %c %u B %s 0x%lx\n", lastExecutedInstruction, ins_disasm->c_str(), isWrite ? 'W' : 'R', size, isWrite ? "to" : "from", addr);

    MemoryAccess ma(lastExecutedInstruction, addr, size, type, std::string(*ins_disasm));
    AccessIndex ai(addr, size);

    /*if(accesses.find(addr) != accesses.end()){
        vector<MemoryAccess> &lst = accesses[addr];
        lst.push_back(ma);
    }
    else{
        vector<MemoryAccess> v;
        v.push_back(ma);
        accesses[addr] = v;
    }*/
 
    PIN_MutexLock(&lock);

    if(fullOverlaps.find(ai) != fullOverlaps.end()){
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

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Image(IMG img, VOID* v){
    if(IMG_IsMainExecutable(img)){
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
        std::ifstream mapping;
        std::ofstream savedMapping("mapBefore.log");

        INT pid = PIN_GetPid();
        std::stringstream mapPath;
        mapPath << "/proc/" << pid << "/maps";
        mapping.open(mapPath.str().c_str());
        std::string cont((std::istreambuf_iterator<char>(mapping)), (std::istreambuf_iterator<char>()));
        savedMapping << cont << endl;
    }
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    ADDRINT stackBase = PIN_GetContextReg(ctxt, REG_STACK_PTR);
    *out << "Stack base of thread " << tid << " is 0x" << std::hex << stackBase << endl;
    threadInfos.insert(std::pair<THREADID, ADDRINT>(tid, stackBase));
}

VOID OnThreadEnd(THREADID tid, CONTEXT* ctxt, INT32 code, VOID* v){

}

VOID Routine(RTN rtn, VOID* v){
    routines << RTN_Name(rtn) << " @ " << std::hex << RTN_Address(rtn) << endl;
}

VOID Instruction(INS ins, VOID* v){
    std::stringstream call_str;

    ADDRINT ins_addr = INS_Address(ins);

    if(ins_addr >= textStart && ins_addr <= textEnd){
        if(INS_IsCall(ins)){
            call_str << std::hex << "0x" << ins_addr << " is a call instruction";
            if(!INS_IsProcedureCall(ins)){
                call_str << ", but is not a procedure call";
            }
            call_str << endl;
        }

        calls << call_str.str();
    }

    UINT32 memoperands = INS_MemoryOperandCount(ins);

    for(UINT32 memop = 0; memop < memoperands; memop++){
        if(INS_MemoryOperandIsWritten(ins, memop) ){
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR) memtrace, IARG_THREAD_ID, IARG_CONTEXT, IARG_UINT32, AccessType::WRITE, IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_PTR, new std::string(INS_Disassemble(ins)), IARG_END);           
        }

        if(INS_MemoryOperandIsRead(ins, memop)){
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR) memtrace, IARG_THREAD_ID, IARG_CONTEXT, IARG_UINT32, AccessType::READ, IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_PTR, new std::string(INS_Disassemble(ins)), IARG_END);
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
    fprintf(trace, "\n===============================================\n");
    fprintf(trace, "MEMORY TRACE END\n");
    fprintf(trace, "===============================================\n\n");

    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = fullOverlaps.begin(); it != fullOverlaps.end(); ++it){
        // Write text file with full overlaps
        set<MemoryAccess> v = it->second;
        memOverlaps << "===============================================" << endl;
        memOverlaps << "0x" << std::hex << it->first.getFirst() << " - " << std::dec << it->first.getSecond() << endl;
        memOverlaps << "===============================================" << endl << endl;
        for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); v_it++){
            memOverlaps << "0x" << std::hex << v_it->getIP() << ": " << v_it->getDisasm() << "\t" << (v_it->getType() == AccessType::WRITE ? "W " : "R ") << std::dec << v_it->getSize() << std::hex << " B @ 0x" << v_it->getAddress() << endl;
        }
        memOverlaps << "===============================================" << endl;
        memOverlaps << "===============================================" << endl << endl << endl << endl << endl;

        // Fill the partial overlaps map
        ADDRINT lastAccessedByte = it->first.getFirst() + it->first.getSecond() - 1;
        std::map<AccessIndex, set<MemoryAccess>>::iterator partialOverlapIterator = fullOverlaps.find(it->first);
        ++partialOverlapIterator;
        ADDRINT accessedAddress = partialOverlapIterator->first.getFirst();
        while(partialOverlapIterator != fullOverlaps.end() && accessedAddress <= lastAccessedByte){
            if(partialOverlaps.find(it->first) != partialOverlaps.end()){
                set<MemoryAccess> &vect = partialOverlaps[it->first];
                vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());
            }
            else{
                set<MemoryAccess> vect;
                vect.insert(partialOverlapIterator->second.begin(), partialOverlapIterator->second.end());
                partialOverlaps[it->first] = vect;
            }

            ++partialOverlapIterator;
            accessedAddress = partialOverlapIterator->first.getFirst();
        }
    }

    std::ofstream overlaps("partialOverlaps.log");
    // Write text file with partial overlaps
    for(std::map<AccessIndex, set<MemoryAccess>>::iterator it = partialOverlaps.begin(); it != partialOverlaps.end(); ++it){
        set<MemoryAccess> v = it->second;
        overlaps << "===============================================" << endl;
        overlaps << "0x" << std::hex << it->first.getFirst() << endl;
        overlaps << "===============================================" << endl;

        for(set<MemoryAccess>::iterator v_it = v.begin(); v_it != v.end(); ++v_it){
            ADDRINT overlapBeginning = v_it->getAddress() - it->first.getFirst();
            overlaps    << "0x" << std::hex << v_it->getIP() << ": " << v_it->getDisasm() << "\t" 
                        << (v_it->getType() == AccessType::WRITE ? "W " : "R ")
                        << "bytes [" << std::dec << overlapBeginning << " ~ " << min(overlapBeginning + v_it->getSize() - 1, it->first.getSecond() - 1) << "]" << endl;
        }
        overlaps << "===============================================" << endl;
        overlaps << "===============================================" << endl << endl << endl << endl << endl;
    }

    fclose(trace);
    routines.close();
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

    trace = fopen(filename.c_str(), "w");
    routines.open("routines.log");
    calls.open("calls.log");
    memOverlaps.open("overlaps.log");
    stackAddress.open("stackAddress.log");
    
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    RTN_AddInstrumentFunction(Routine, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    
    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MemTrace" << endl;
    cerr << "See file " << filename << " for analysis results" << endl;
    cerr <<  "===============================================" << endl;

    fprintf(trace, "===============================================\n");
    fprintf(trace, "MEMORY TRACE START\n");
    fprintf(trace, "===============================================\n\n");
    fprintf(trace, "<Program Counter>: <Instruction_Disasm> => R/W <Address>\n\n");

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
