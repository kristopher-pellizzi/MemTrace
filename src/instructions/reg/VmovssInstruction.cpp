#include "VmovssInstruction.h"

void VmovssInstruction::operator()(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs){
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    auto srcIter = srcRegs->begin();
    REG src1 = *(srcIter++);
    REG src2 = *srcIter;
    REG dstReg = *dstRegs->begin();
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    if(!registerFile.isUninitialized(src1) && !registerFile.isUninitialized(src2)){
        registerFile.setAsInitialized(dstReg);
        return;
    }

    uint8_t* src1Status = registerFile.getContentStatus(src1);
    uint8_t* src2Status = registerFile.getContentStatus(src2);
    unsigned shadowSize = registerFile.getShadowSize(dstReg);
    uint8_t* dstStatus = src1Status;

    uint8_t* srcPtr = src2Status + shadowSize - 1;
    uint8_t* dstPtr = dstStatus + shadowSize - 1;
    uint8_t dstMask = 0xff;
    dstMask <<= 4;
    uint8_t srcMask = ~dstMask;

    *dstPtr &= dstMask;
    *dstPtr |= (*srcPtr & srcMask); 

    registerFile.setAsInitialized(dstReg, dstStatus);

    propagatePendingReads(srcRegs, dstRegs);

    free(src1Status);
    free(src2Status);
}