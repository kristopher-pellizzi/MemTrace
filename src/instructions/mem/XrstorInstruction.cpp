#include "XrstorInstruction.h"

void XrstorInstruction::operator()(MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    REG dstReg = *dstRegs->begin();
    uint8_t* uninitializedInterval = ma.getUninitializedInterval();
    uint8_t* srcStatus = cutUselessBits(uninitializedInterval, ma.getAddress(), ma.getSize());
    unsigned dstShadowSize = registerFile.getShadowSize(dstReg);
    unsigned srcShadowSize = ma.getSize();
    srcShadowSize = srcShadowSize % 8 != 0 ? (srcShadowSize / 8) + 1 : srcShadowSize / 8;
    uint8_t* regStatus = registerFile.getContentStatus(dstReg);

    /*
        Being an XRSTOR instruction, the byte size of the memory access is equal or at most lower than
        the destination register size.
        So, just copy all the shadow bytes read from memory into the higher bits of the register.
        E.g. if ymm0 has been stored, only the higher 16 bytes have been replicated, because the lower ones
        were stored by storing xmm0 (this is how XSAVE works, see Intel's manual for details).
        Therefore, XRSTOR just reads the memory location where ymm0 has been saved and restores the higher bytes
        of the register, while the lower bytes are restored by restoring xmm0.
    */
    uint8_t* dstStatus = (uint8_t*) malloc(sizeof(uint8_t) * dstShadowSize);
    uint8_t* srcPtr = srcStatus;
    uint8_t* dstPtr = dstStatus; 
    unsigned i = 0;

    for(i = 0; i < srcShadowSize; ++i, ++dstPtr, ++srcPtr){
        *dstPtr = *srcPtr;
    }

    srcPtr = regStatus + i;

    for(i = srcShadowSize; i < dstShadowSize; ++i, ++dstPtr, ++srcPtr){
        *dstPtr = *srcPtr;
    }

    registerFile.setAsInitialized(dstReg, dstStatus);

    addPendingRead(dstRegs, ma);

    free(srcStatus);
    free(regStatus);
    free(dstStatus);
}