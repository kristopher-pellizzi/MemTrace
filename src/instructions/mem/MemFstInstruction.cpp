#include "MemFstInstruction.h"

void MemFstInstruction::operator()(MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs){
    /*
        Note that this is an over-approximation of the real behavior of the instruction.
        Indeed, when we copy the value to a smaller register, the value is actually converted from an
        extended precision format (80 bit FP value) to a single or double precision format (respectively 32 or 64 bits
        FP value).
        In order to avoid complex computation which may greatly slow down the entire execution, we just set the whole
        single or double precision value, if the original extended precision value has even only 1 uninitialized byte.
        Of course, this may lead to some false positives, but they should be very limited in number.
    */

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    REG srcReg = *srcRegs->begin();
    bool srcIsUninitialized = registerFile.isUninitialized(srcReg);
    UINT32 memSize = ma.getSize();
    unsigned offset = ma.getAddress() % 8;

    // If written memory location is in single or double precision format, a conversion is implicitly done by the processor.
    // Consider the whole written value as uninitialized if the src is.
    if(memSize < 10){
        if(srcIsUninitialized){
            unsigned dstShadowSize = memSize + offset;
            dstShadowSize = dstShadowSize % 8 != 0 ? (dstShadowSize / 8) + 1 : dstShadowSize / 8;
            uint8_t* dstStatus = (uint8_t*) calloc(dstShadowSize, sizeof(uint8_t));
            set_as_initialized(ma.getAddress(), memSize, dstStatus);
            free(dstStatus);
        }
        else{
            set_as_initialized(ma.getAddress(), memSize);
        }
    }
    else{
        unsigned srcShadowSize = registerFile.getShadowSize(srcReg);
        unsigned srcByteSize = registerFile.getByteSize(srcReg);
        uint8_t* srcStatus = registerFile.getContentStatus(srcReg);
        // Since srcReg is a FP register, it is 80 bit wide. So, the 6 most significative bits of its status are not related
        // to the register itself and must be set to 0.
        *srcStatus >>= 6;
        uint8_t* dstStatus = srcStatus;
        if(offset != 0)
            dstStatus = addOffset(srcStatus, offset, &srcShadowSize, srcByteSize);

        // Now the dstStatus is ready to be written as is into shadow memory
        set_as_initialized(ma.getAddress(), memSize, dstStatus);

        free(srcStatus);
        free(dstStatus);
    }
}