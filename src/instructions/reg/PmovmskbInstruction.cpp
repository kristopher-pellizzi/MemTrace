#include "PmovmskbInstruction.h"

void PmovmskbInstruction::operator()(OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    
    REG srcReg = *srcRegs->begin();
    REG dstReg = *dstRegs->begin();

    if(registerFile.isUnknownRegister(dstReg))
        return;

    if(registerFile.isUnknownRegister(srcReg) || !registerFile.isUninitialized(srcReg)){
        registerFile.setAsInitialized(dstReg);
        return;
    }

    unsigned srcShadowSize = registerFile.getShadowSize(srcReg);
    unsigned dstShadowSize = registerFile.getShadowSize(dstReg);
    uint8_t* dstStatus = (uint8_t*) calloc(dstShadowSize, sizeof(uint8_t));
    uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
    uint8_t* srcPtr = srcStatus + srcShadowSize - 1;
    uint8_t* dstPtr = dstStatus + dstShadowSize - 1;
    uint8_t shiftCount = 0;

    for(unsigned i = 0; i < srcShadowSize; ++i){
        bool isUninitialized = *srcPtr != 0xff;
        
        if(!isUninitialized){
            *dstPtr |= ((uint8_t) 1 << shiftCount);
        }

        ++shiftCount;

        if(shiftCount >= 8){
            shiftCount = 0;
            --dstPtr;
        }

        --srcPtr;
    }

    unsigned dstByteSize = registerFile.getByteSize(dstReg);
    unsigned maxShiftCount = 8;

    if(shiftCount > 0){
        while(shiftCount < std::min(maxShiftCount, dstByteSize)){
            *dstPtr |= ((uint8_t) 1 << shiftCount);
            ++shiftCount;
        }
        --dstPtr;
    }

    for(unsigned i = srcShadowSize; i < dstShadowSize; ++i){
        *dstPtr = 0xff;
        --dstPtr;
    }

    registerFile.setAsInitialized(dstReg, dstStatus);
    free(dstStatus);
    free(srcStatus);

    propagatePendingReads(srcRegs, dstRegs);
}