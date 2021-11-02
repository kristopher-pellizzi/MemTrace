#include "pin.H"
#include "PendingReads.h"
#include "InstructionClassification.h"
#include <list>
#include <map>
#include <iostream>

using std::list;
using std::map;

#ifndef DSTREGSCHECKER
#define DSTREGSCHECKER

extern map<OPCODE, unsigned> checkDestSize;

// Checks which registers are completely overwritten when registers in |dstRegs| are completely overwritten
// and removes their pending reads from |pendingUninitializedReads| map
VOID checkDestRegisters(list<REG>* dstRegs, OPCODE opcode);

// Checks which registers are completely overwritten when the first |bits| bits are overwritten for registers in |dstRegs| 
// and removes their pending reads from |pendingUninitializedReads| map
VOID checkDestRegisters(list<REG>* dstRegs, OPCODE opcode, unsigned bits);

#endif //DSTREGSCHECKER