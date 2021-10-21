#include "ConvertInstruction.h"

/*
    Since the sign extension completely initialized the dst register, there's no need to call |propagatePendingReads|
*/
void ConvertInstruction::operator()(OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    // This kind of instruction has only 1 dst register, which can be either DX, EDX or RDX, according to the size of 
    // the src register
    REG dstReg = *dstRegs->begin();

    if(registerFile.isUnknownRegister(dstReg))
        return;

    registerFile.setAsInitialized(dstReg);
}