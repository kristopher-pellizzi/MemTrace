#include "MemVpbroadcastInstruction.h"

static void broadcast(uint8_t* regData, REG dstReg, unsigned bcSize){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    unsigned dstShadowSize = registerFile.getShadowSize(dstReg); 
    uint8_t lsbStatus = *regData;
    uint8_t* bytes = (uint8_t*) malloc(sizeof(uint8_t) * bcSize);

    for(unsigned i = 0; i < bcSize; ++i){
        bytes[i] = lsbStatus & (1 << i);;
    }

    bool isUninitialized = false;

    for(unsigned i = 0; !isUninitialized && i < bcSize; ++i){
        if(bytes[i] != (1 << i)){
            isUninitialized = true;
        }
    }

    if(isUninitialized){
        uint8_t* dstStatus = (uint8_t*) malloc(sizeof(uint8_t) * dstShadowSize);
        uint8_t* dstPtr = dstStatus + dstShadowSize - 1;
        uint8_t dstByte = 0;

        for(unsigned i = 0; i < 8; i += bcSize){
            uint8_t tmpByte = 0;
            for(unsigned j = 0; j < bcSize; ++j){
                tmpByte |= bytes[j];
            }
            dstByte |= (tmpByte << i);
        }

        for(unsigned i = 0; i < dstShadowSize; ++i){
            *dstPtr = dstByte;
            --dstPtr;
        }

        registerFile.setAsInitialized(dstReg, dstStatus);
        free(dstStatus);
    }
    else{
        registerFile.setAsInitialized(dstReg);
    }
}

void MemVpbroadcastInstruction::operator()(MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    REG dstReg = *dstRegs->begin();

    if(registerFile.isUnknownRegister(dstReg))
        return;

    unsigned broadcastSize = 0;

    switch(ma.getOpcode()){
        case XED_ICLASS_VPBROADCASTB:
            broadcastSize = 1;
            break;

        case XED_ICLASS_VPBROADCASTW:
            broadcastSize = 2;
            break;

        case XED_ICLASS_VPBROADCASTD:
            broadcastSize = 4;
            break;

        case XED_ICLASS_VPBROADCASTQ:
            broadcastSize = 8;
            break;
    }

    uint8_t* uninitializedInterval = ma.getUninitializedInterval();
    // Note that read memory can be either 8, 16, 32 or 64 bits only
    uint8_t* regData = cutUselessBits(uninitializedInterval, ma.getAddress(), ma.getSize());

    broadcast(regData, dstReg, broadcastSize);
    free(regData);

    addPendingRead(dstRegs, ma);
}