#include "DefaultLoadInstruction.h"

static uint8_t* expandData(uint8_t* data, MemoryAccess& ma, unsigned shadowSize, unsigned regByteSize, unsigned regShadowSize){
    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t) * regShadowSize);
    uint8_t* dst = ret + regShadowSize - 1;
    UINT32 accessSize = ma.getSize();

    if(accessSize < 8){
        uint8_t mask = ~ (0xff << accessSize);
        uint8_t toReplicate = *data & mask;

        for(int i = 8 - accessSize; i > 0; i -= accessSize){
            toReplicate |= (toReplicate << accessSize);
        }

        for(unsigned i = 0; i < regShadowSize; ++i){
            *dst-- = toReplicate;
        }
    }
    else{
        unsigned left = regShadowSize;
        unsigned leftSrc = shadowSize;
        uint8_t* src = data + shadowSize - 1;
        while(left > 0){
            if(leftSrc == 0)
                src = data + shadowSize - 1;
            *dst-- = *src--;
            --left;
            --leftSrc;
        }
    }

    return ret;
}

RETTYPE DefaultLoadInstruction::operator() ARGS{
    // If there are no destination registers, there's nothing to do
    if(regs->size() == 0)
        return;

    uint8_t* uninitializedInterval = ma.getUninitializedInterval();
    uint8_t* regData = cutUselessBits(uninitializedInterval, ma.getAddress(), ma.getSize());
    UINT32 shadowSize = ma.getSize() + ma.getAddress() % 8;
    shadowSize = shadowSize % 8 != 0 ? (shadowSize / 8) + 1 : shadowSize / 8;

    for(auto iter = regs->begin(); iter != regs->end(); ++iter){
        unsigned regShadowSize = ShadowRegisterFile::getInstance().getShadowSize(*iter);
        // Register |*iter| has no corresponding shadow register
        if(regShadowSize == (unsigned) -1){
            #ifdef DEBUG
            cerr << "Register " << REG_StringShort(*iter) << " has no corresponding shadow register" << endl;
            #endif
            continue;
        }

        // At this point, we are already sure that the call to |getByteSize| won't be -1, because otherwise also the 
        // previous call to |getShadowSize| would have returned -1, thus not reaching this point
        unsigned regByteSize = ShadowRegisterFile::getInstance().getByteSize(*iter);
        uint8_t* curr_data = regData + shadowSize - regShadowSize;

        if(regByteSize < ma.getSize()){
            cerr 
                << "[DefaultLoadInstruction] Warning: writing shadow register " 
                << ShadowRegisterFile::getInstance().getName(*iter) 
                << ". Register size (" << regByteSize << ") is lower than the read memory (" << ma.getSize() << ")." << endl;
            cerr    
                << "It's possible that the information about uninitialized bytes stored into the shadow register is "
                << "not correct. Probably it's required to implement an ad-hoc instruction handler." << endl;
        }
        else if(regByteSize > ma.getSize()){
            cerr 
                << "[DefaultLoadInstruction] Warning: writing shadow register " 
                << ShadowRegisterFile::getInstance().getName(*iter) 
                << ". Register size (" << regByteSize << ") is higher than the read memory (" << ma.getSize() << ")." << endl;
            cerr    
                << "It's possible that the information about uninitialized bytes stored into the shadow register is "
                << "not correct. Probably it's required to implement an ad-hoc instruction handler." << endl;
           
            uint8_t* expandedData = expandData(regData, ma, shadowSize, regByteSize, regShadowSize);
            curr_data = expandedData;
            ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
            continue;
        }

        ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
    }
    free(regData);
}