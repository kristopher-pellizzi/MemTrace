#include <iostream>
#include <fstream>
#include "pin.H"
#include "ShadowRegisterFile.h"
#include "misc/PendingReads.h"

using std::cerr;
using std::endl;
using std::pair;

#ifndef REGINSTRUCTIONEMULATOR
#define REGINSTRUCTIONEMULATOR

class RegInstructionEmulator{
    public:
        virtual ~RegInstructionEmulator();
        virtual void operator() (OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs) = 0;
};

#endif //REGINSTRUCTIONEMULATOR