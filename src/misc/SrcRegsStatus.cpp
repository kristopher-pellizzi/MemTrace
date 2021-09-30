#include "SrcRegsStatus.h"

// Returns a pair containing a pointer to the src registers status and the size in bytes of the biggest src register
pair<uint8_t*, unsigned> getSrcRegsStatus(set<REG>* srcRegs){
    pair<uint8_t*, unsigned> ret(NULL, 0);
    uint8_t* ptr = NULL;
    unsigned byteSize = 0;
    unsigned shadowSize = 0;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); ++iter){
        unsigned regByteSize = registerFile.getByteSize(*iter);
        unsigned regShadowSize = registerFile.getShadowSize(*iter);

        if(regByteSize > byteSize)
            byteSize = regByteSize;

        // Note that this should happen only rarely, theoretically only once for the initial allocation
        // of |ptr|
        if(regShadowSize > shadowSize){
            uint8_t* newPtr = (uint8_t*) malloc(sizeof(uint8_t) * regShadowSize);
            // Set the new allocated bytes to 0xff
            for(unsigned i = 0; i < regShadowSize - shadowSize; ++i){
                *(newPtr + i) = 0xff;
            }

            // Set the already allocated bytes to their old value
            unsigned j = 0;
            for(unsigned i = regShadowSize - shadowSize; i < regShadowSize; ++i, ++j){
                *(newPtr + i) = *(ptr + j);
            }

            free(ptr);
            ptr = newPtr;
            shadowSize = regShadowSize;
        }

        uint8_t* regStatus = registerFile.getContentStatus(*iter);
        for(unsigned i = 0; i < shadowSize; ++i){
            *(ptr + i) &= *(regStatus + i);
        }
        free(regStatus);
    }

    ret.first = ptr;
    ret.second = byteSize;

    return ret;
}