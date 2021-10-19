#include "../../RegInstructionEmulator.h"
#include "../../misc/SrcRegsStatus.h"

#ifndef DEFAULTPROPAGATEINSTRUCTION
#define DEFAULTPROPAGATEINSTRUCTION

class DefaultPropagateInstruction : public RegInstructionEmulator{
    private:
        /*
            This map contains the opcodes of instructions which have an asymmetric src/dst size, but work fine with
            the default loader (e.g. movd/movq may load to a register whose size is higher than the read memory.
            In that case, the upper bytes of the register are set to 0, thus initializing the whole register.
            This is compatible with the DefaultLoadInstruction emulator.)
        */
        std::set<OPCODE> verifiedInstructions;

        void initVerifiedInstructions();

    public:
        DefaultPropagateInstruction();
        void operator() (OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs) override;
};

#endif //DEFAULTPROPAGATEINSTRUCTION