#include <cstring>
#include "SrcRegsStatus.h"

// Returns a pair containing a pointer to the src registers status and the size in bytes of the biggest src register
RegsStatus getSrcRegsStatus(list<REG>* srcRegs){
    uint8_t* ptr = NULL;
    unsigned byteSize = 0;
    unsigned shadowSize = 0;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    bool allRegistersInitialized = true;

    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); ++iter){
        // If this register does not have an associated shadow register,
        // we can't propagate anything
        if(registerFile.isUnknownRegister(*iter))
            continue;
        unsigned regByteSize = registerFile.getByteSize(*iter);
        unsigned regShadowSize = registerFile.getShadowSize(*iter);

        // If register is completely initialized, simply update the size in order to save time
        if(!registerFile.isUninitialized(*iter)){
            if(regByteSize > byteSize)
                byteSize = regByteSize;
            
            if(regShadowSize > shadowSize)
                shadowSize = regShadowSize;
        }
        else{

            allRegistersInitialized = false;
            if(regByteSize > byteSize)
                byteSize = regByteSize;

            // Up to now, registers were completely initialized.
            // Initialize the status ptr to be completely filled with 1.
            if(ptr == NULL){
                ptr = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);
                memset(ptr, 0xff, shadowSize);
            }

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
    }

    // If the condition holds, we did not allocate any pointer yet. Allocate the status ptr and fill it with 1
    if(allRegistersInitialized){
        ptr = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);
        memset(ptr, 0xff, shadowSize);
    }

    RegsStatus ret(ptr, byteSize, shadowSize, allRegistersInitialized);

    return ret;
}


// IMPLEMENTATION OF RegsStatus class
RegsStatus::RegsStatus(uint8_t* status, unsigned byteSize, unsigned shadowSize, bool allInitialized) :
    status(status),
    byteSize(byteSize),
    shadowSize(shadowSize),
    allInitialized(allInitialized)
{}


RegsStatus::~RegsStatus(){
    free(status);
}


uint8_t* RegsStatus::getStatus(){
    return status;
}

unsigned RegsStatus::getByteSize(){
    return byteSize;
}

unsigned RegsStatus::getShadowSize(){
    return shadowSize;
}

bool RegsStatus::isAllInitialized(){
    return allInitialized;
}