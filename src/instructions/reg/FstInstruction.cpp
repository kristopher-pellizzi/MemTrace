#include "FstInstruction.h"

void FstInstruction::operator()(OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    /*
        Note that this is a register copy instruction where both the source and the destination registers are x87
        FPU registers, i.e. 80-bit registers containing floating point values stored in extended precision format.
    */

    REG srcReg = *srcRegs->begin();
    REG dstReg = *dstRegs->begin();

    if(registerFile.isUninitialized(srcReg)){
        uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
        registerFile.setAsInitialized(dstReg, srcStatus);
        free(srcStatus);
    }
    else{
        registerFile.setAsInitialized(dstReg);
    }
}