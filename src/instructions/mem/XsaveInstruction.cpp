#include "XsaveInstruction.h"

/*
    Function called when register size is higher than the write access size.
    It returns a pointer containing the bitmap of the most significative bits.
    E.g. register size = 32, ma.size() = 16 => take the 16 most significative bits of the bitmap

    Note that this is done because of how XSAVE works. For instance, ince XMM registers correspond to the low part 
    of YMM registers, XSAVE stores XMM registers and only the high part of YMM registers to avoid storing the 
    same state twice.
*/
static uint8_t* getDstStatus(uint8_t* srcStatus, MemoryAccess& ma){
    unsigned dstShadowSize = ma.getSize() / 8;
    uint8_t* ret = (uint8_t*) calloc(dstShadowSize, sizeof(uint8_t));

    for(unsigned i = 0; i < dstShadowSize; ++i){
        *(ret + i) = *(srcStatus + i);
    }

    return ret;
}

void XsaveInstruction::operator()(MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs){
    if(srcRegs == NULL){
        set_as_initialized(ma.getAddress(), ma.getSize());
        return;
    }

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    REG srcReg = *srcRegs->begin();
    uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
    uint8_t* dstStatus = srcStatus;
    unsigned byteSize = ma.getSize();

    if(registerFile.getByteSize(srcReg) > byteSize){
        dstStatus = getDstStatus(srcStatus, ma);
    }

    unsigned offset = ma.getAddress() % 8;
    if(offset != 0){
        unsigned srcShadowSize = byteSize / 8;
        uint8_t* tmp = addOffset(dstStatus, offset, &srcShadowSize, byteSize);
        free(dstStatus);
        dstStatus = tmp;
    }

    set_as_initialized(ma.getAddress(), byteSize, dstStatus);
    free(dstStatus);
    if(srcStatus != dstStatus)
        free(srcStatus);

    storePendingReads(srcRegs, ma);
}