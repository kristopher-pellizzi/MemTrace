#include "../../MemInstructionEmulator.h"
#include "../../misc/SrcRegsStatus.h"

#ifndef DEFAULTLOADINSTRUCTION
#define DEFAULTLOADINSTRUCTION

class DefaultLoadInstruction : public MemInstructionEmulator{
    void operator() (MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs) override;
};

#endif //DEFAULTLOADINSTRUCTION