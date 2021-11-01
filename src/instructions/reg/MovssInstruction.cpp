#include "MovssInstruction.h"

void MovssInstruction::operator()(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs){
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    REG srcReg = *srcRegs->begin();
    REG dstReg = *dstRegs->begin();
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
    uint8_t* dstStatus = registerFile.getContentStatus(dstReg);
    unsigned shadowSize = registerFile.getShadowSize(dstReg);
    uint8_t* srcPtr = srcStatus + shadowSize - 1;
    uint8_t* dstPtr = dstStatus + shadowSize - 1;
    uint8_t dstMask = 0xff;
    dstMask <<= 4;
    uint8_t srcMask = ~ dstMask;
    *dstPtr &= dstMask;
    *dstPtr |= (*srcPtr & srcMask);

    registerFile.setAsInitialized(dstReg, opcode, dstStatus);

    propagatePendingReads(srcRegs, dstRegs);

    free(srcStatus);
    free(dstStatus);
}