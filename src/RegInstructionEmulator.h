#include <iostream>
#include <fstream>
#include "pin.H"
#include "ShadowRegisterFile.h"

using std::cerr;
using std::endl;
using std::pair;

#ifndef REGINSTRUCTIONEMULATOR
#define REGINSTRUCTIONEMULATOR

class RegInstructionEmulator{
    public:
        virtual ~RegInstructionEmulator();
        virtual void operator() (OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs) = 0;
};

#endif //REGINSTRUCTIONEMULATOR