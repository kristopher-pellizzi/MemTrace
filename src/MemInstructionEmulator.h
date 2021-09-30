#include <iostream>
#include <fstream>
#include "pin.H"
#include "MemoryAccess.h"
#include "ShadowRegisterFile.h"

using std::cerr;
using std::endl;
using std::pair;

#ifndef MEMINSTRUCTIONEMULATOR
#define MEMINSTRUCTIONEMULATOR

class MemInstructionEmulator{
    protected:
        uint8_t* cutUselessBits(uint8_t* uninitializedInterval, ADDRINT addr, UINT32 byteSize);

    public:
        virtual void operator() (MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs) = 0;
};

#endif //MEMINSTRUCTIONEMULATOR