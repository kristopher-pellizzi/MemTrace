#include <iostream>
#include <fstream>
#include "pin.H"
#include "MemoryAccess.h"
#include "ShadowRegisterFile.h"
#include "ShadowMemory.h"
#include "misc/MemoryStatus.h"
#include "misc/PendingReads.h"

using std::cerr;
using std::endl;
using std::pair;

#ifndef MEMINSTRUCTIONEMULATOR
#define MEMINSTRUCTIONEMULATOR

class MemInstructionEmulator{

    public:
        virtual ~MemInstructionEmulator();
        virtual void operator() (MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs) = 0;
};

#endif //MEMINSTRUCTIONEMULATOR