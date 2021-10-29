#include "DefaultLoadInstruction.h"

using std::ofstream;

static uint8_t* expandData(uint8_t* data, MemoryAccess& ma, unsigned shadowSize, unsigned regByteSize, unsigned regShadowSize){
    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t) * regShadowSize);
    uint8_t* src = data;
    
    unsigned diff = regShadowSize - shadowSize;
    for(unsigned i = 0; i < diff; ++i){
        *(ret + i) = 0xff;
    }
    for(unsigned i = diff; i < regShadowSize; ++i, ++src){
        *(ret + i) = *src;
    }

    return ret;
}

DefaultLoadInstruction::DefaultLoadInstruction(){
    initVerifiedInstructions();
}

void DefaultLoadInstruction::initVerifiedInstructions(){
    verifiedInstructions.insert(XED_ICLASS_MOVD);
    verifiedInstructions.insert(XED_ICLASS_MOVQ);
    verifiedInstructions.insert(XED_ICLASS_VMOVD);
    verifiedInstructions.insert(XED_ICLASS_VMOVQ);
    verifiedInstructions.insert(XED_ICLASS_MOVZX);
    verifiedInstructions.insert(XED_ICLASS_MOVSX);
    verifiedInstructions.insert(XED_ICLASS_MOVSXD);
    verifiedInstructions.insert(XED_ICLASS_MOVSD_XMM);
}

void  DefaultLoadInstruction::operator() (MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs){
    // If there are no destination registers, there's nothing to do
    if(dstRegs == NULL)
        return;

    ofstream warningOpcodes;
    uint8_t* uninitializedInterval = ma.getUninitializedInterval();
    uint8_t* regData = cutUselessBits(uninitializedInterval, ma.getAddress(), ma.getSize());
    UINT32 shadowSize = ma.getSize();
    shadowSize = shadowSize % 8 != 0 ? (shadowSize / 8) + 1 : shadowSize / 8;
    bool isVerifiedInstruction = verifiedInstructions.find(ma.getOpcode()) != verifiedInstructions.end();

    // Get the content status of all source registers and merge it with status loaded from memory (bitwise AND)
    if(srcRegs != NULL){
        RegsStatus srcStatus = getSrcRegsStatus(srcRegs);
        uint8_t* srcStatusPtr = srcStatus.getStatus();
        unsigned srcByteSize = srcStatus.getByteSize();
        unsigned srcShadowSize = srcStatus.getShadowSize();

        if(!isVerifiedInstruction && srcByteSize != ma.getSize()){
            #ifdef DEBUG
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
            #endif

            warningOpcodes.open("warningOpcodes.log", std::ios::app);
            warningOpcodes 
                << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) 
                << " raised a warning 'cause source register size is "
                << (srcByteSize < ma.getSize() ? "LOWER" : "HIGHER")
                << " than memory size" << endl;
        }

        if(!srcStatus.isAllInitialized()){           
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
        }
    }


    // Set the status for each destination register
    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        if(ShadowRegisterFile::getInstance().isUnknownRegister(*iter)){
            continue;
        }
            
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
            if(!isVerifiedInstruction){
                if(!warningOpcodes.is_open()){
                    warningOpcodes.open("warningOpcodes.log", std::ios::app);
                }

                #ifdef DEBUG
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
                #endif

                warningOpcodes << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) << " raised a warning 'cause register size is LOWER than memory size" << endl;
            }
        }
        else if(regByteSize > ma.getSize()){
            if(!isVerifiedInstruction){
                #ifdef DEBUG
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
                #endif
                
                warningOpcodes << LEVEL_CORE::OPCODE_StringShort(ma.getOpcode()) << " raised a warning 'cause register size is HIGHER than memory size" << endl;
            }

            uint8_t* expandedData = expandData(regData, ma, shadowSize, regByteSize, regShadowSize);
            curr_data = expandedData;
            ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
            free(expandedData);
            continue;
        }

        ShadowRegisterFile::getInstance().setAsInitialized(*iter, curr_data);
    }
    warningOpcodes.close();
    free(regData);

    addPendingRead(dstRegs, ma);
}