#include "MovsdInstruction.h"

void MovsdInstruction::operator()(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs){
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    REG srcReg = *srcRegs->begin();
    REG dstReg = *dstRegs->begin();
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
    uint8_t* dstStatus = registerFile.getContentStatus(dstReg);
    unsigned shadowSize = registerFile.getShadowSize(dstReg); // It is the same for both the registers
    uint8_t* srcPtr = srcStatus + shadowSize - 1;
    uint8_t* dstPtr = dstStatus + shadowSize - 1;
    
    /*
        This instruction copies a scalar double-precision FP value from an XMM register to another XMM register.
        A double-precision value is represented with 64 bits in memory (i.e. 8 bytes), so its shadow size is 1 byte.
        So, we simply need to copy the least significant byte of the src register into the least significant byte of the dst 
        register, and leave everything else untouched.
    */
    *dstPtr = *srcPtr;

    registerFile.setAsInitialized(dstReg, dstStatus);

    propagatePendingReads(srcRegs, dstRegs);
}