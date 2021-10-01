#include "DefaultLoadInstruction.h"

using std::ofstream;

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

void  DefaultLoadInstruction::operator() (MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs){
    // If there are no destination registers, there's nothing to do
    if(dstRegs == NULL)
        return;

    ofstream warningOpcodes;
    uint8_t* uninitializedInterval = ma.getUninitializedInterval();
    uint8_t* regData = cutUselessBits(uninitializedInterval, ma.getAddress(), ma.getSize());
    UINT32 shadowSize = ma.getSize() + ma.getAddress() % 8;
    shadowSize = shadowSize % 8 != 0 ? (shadowSize / 8) + 1 : shadowSize / 8;

    // Get the content status of all source registers and merge it with status loaded from memory (bitwise AND)
    if(srcRegs != NULL){
        RegsStatus srcStatus = getSrcRegsStatus(srcRegs);
        uint8_t* srcStatusPtr = srcStatus.getStatus();
        unsigned srcByteSize = srcStatus.getByteSize();
        unsigned srcShadowSize = srcStatus.getShadowSize();

        if(srcByteSize != ma.getSize()){
            cerr 
                << "[DefaultLoadInstruction] Warning: reading source registers." << endl;
            cerr 
                << "Instruction : " << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) 
                << "." << endl;
            cerr
                << "Register size (" << std::dec << srcByteSize << ") is "
                << (srcByteSize < ma.getSize() ? "lower" : "higher") 
                << " than the read memory (" << ma.getSize() << ")." << endl;
            cerr    
                << "It's possible that the information about uninitialized bytes stored into the shadow register is "
                << "not correct. Probably it's required to implement an ad-hoc instruction handler." << endl << endl;

            warningOpcodes.open("warningOpcodes.log", std::ios::app);
            warningOpcodes 
                << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) 
                << " raised a warning 'cause source register size is "
                << (srcByteSize < ma.getSize() ? "LOWER" : "HIGHER")
                << " than memory size" << endl;
        }
            
        uint8_t* data_ptr = regData;
        unsigned minShadowSize = shadowSize;

        if(srcShadowSize > shadowSize){
            srcStatusPtr += (srcShadowSize - shadowSize);
            minShadowSize = shadowSize;
        }
        else if(srcShadowSize < shadowSize){
            data_ptr += (shadowSize - srcShadowSize);
            minShadowSize = srcShadowSize;
        }

        for(unsigned i = 0; i < minShadowSize; ++i){
            *(data_ptr + i) &= *(srcStatusPtr + i);
        }

        free(srcStatusPtr);
    }


    // Set the status for each destination register
    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
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
            if(!warningOpcodes.is_open()){
                warningOpcodes.open("warningOpcodes.log", std::ios::app);
            }

            cerr 
                << "[DefaultLoadInstruction] Warning: writing shadow register " 
                << ShadowRegisterFile::getInstance().getName(*iter) 
                << "." << endl;
            cerr 
                << "Instruction : " << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) 
                << "." << endl;
            cerr
                << "Register size (" << std::dec << regByteSize << ") is lower than the read memory (" << ma.getSize() << ")." << endl;
            cerr    
                << "It's possible that the information about uninitialized bytes stored into the shadow register is "
                << "not correct. Probably it's required to implement an ad-hoc instruction handler." << endl << endl;

            warningOpcodes << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) << " raised a warning 'cause register size is LOWER than memory size" << endl;
        }
        else if(regByteSize > ma.getSize()){
            cerr 
                << "[DefaultLoadInstruction] Warning: writing shadow register " 
                << ShadowRegisterFile::getInstance().getName(*iter) 
                << "." << endl;
            cerr 
                << "Instruction's opcode: " << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode())
                << "." << endl;
            cerr 
                << "Register size (" << std::dec << regByteSize << ") is higher than the read memory (" << ma.getSize() << ")." << endl;
            cerr    
                << "It's possible that the information about uninitialized bytes stored into the shadow register is "
                << "not correct. Probably it's required to implement an ad-hoc instruction handler." << endl << endl;

            warningOpcodes << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) << " raised a warning 'cause register size is HIGHER than memory size" << endl;
           
            uint8_t* expandedData = expandData(regData, ma, shadowSize, regByteSize, regShadowSize);
            curr_data = expandedData;
            ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
            continue;
        }

        ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
    }
    warningOpcodes.close();
    free(regData);
}