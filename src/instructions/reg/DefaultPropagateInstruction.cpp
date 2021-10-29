#include "DefaultPropagateInstruction.h"

DefaultPropagateInstruction::DefaultPropagateInstruction(){
    initVerifiedInstructions();
}

void DefaultPropagateInstruction::initVerifiedInstructions(){
    verifiedInstructions.insert(XED_ICLASS_MOVD);
    verifiedInstructions.insert(XED_ICLASS_MOVQ);
    verifiedInstructions.insert(XED_ICLASS_VMOVD);
    verifiedInstructions.insert(XED_ICLASS_VMOVQ);
    verifiedInstructions.insert(XED_ICLASS_MOVZX);
    verifiedInstructions.insert(XED_ICLASS_MOVSX);
    verifiedInstructions.insert(XED_ICLASS_MOVSXD);
    verifiedInstructions.insert(XED_ICLASS_CBW);
    verifiedInstructions.insert(XED_ICLASS_CWDE);
    verifiedInstructions.insert(XED_ICLASS_CDQE);
    verifiedInstructions.insert(XED_ICLASS_MOVSD_XMM);
}

void DefaultPropagateInstruction::operator() (OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs){
    // There's nothing to propagate
    if(dstRegs == NULL)
        return;


    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    // If there are dst regs, but not src regs, it is probably loading an immediate to the dst registers, so just set them
    // all as completely initialized
    if(srcRegs == NULL){
        for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
            registerFile.setAsInitialized(*iter);
        }
        return;
    }

    RegsStatus srcStatus = getSrcRegsStatus(srcRegs);

    uint8_t* srcStatusContent = srcStatus.getStatus();
    unsigned srcByteSize = srcStatus.getByteSize();
    unsigned srcShadowSize = srcStatus.getShadowSize();
    bool isVerifiedInstruction = verifiedInstructions.find(opcode) != verifiedInstructions.end();

    std::ofstream warningOpcodes;

    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        if(registerFile.isUnknownRegister(*iter))
            continue;
            
        unsigned byteSize = registerFile.getByteSize(*iter);

        if(!isVerifiedInstruction && srcByteSize != byteSize){
            #ifdef DEBUG
            cerr 
                << "[DefaultPropagateInstruction] Warning: propagating bytes into "
                << registerFile.getName(*iter) << "." << endl;

            cerr 
                << "Instruction : " << LEVEL_CORE::OPCODE_StringShort(opcode) 
                << "." << endl;

            cerr 
                << "Source registers have a size "
                << (srcByteSize < byteSize ? "lower" : "higher") 
                << " than destination register." << endl;
            #endif

            warningOpcodes.open("warningOpcodes.log", std::ios::app);
            warningOpcodes 
                << LEVEL_CORE::OPCODE_StringShort(opcode) 
                << " raised a warning 'cause source register size is "
                << (srcByteSize < byteSize ? "LOWER" : "HIGHER")
                << " than destination register size" << endl;
        }

        if(srcStatus.isAllInitialized()){
            registerFile.setAsInitialized(*iter);
        }
        else{
            unsigned shadowSize = registerFile.getShadowSize(*iter);
            uint8_t* data = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);


            // Consider the excessive bytes as initialized 
            // (e.g. if srcShadowSize is 8, and shadowSize is 16, consider the 8 most significant shadow bytes of the 
            // destination registers as initialized)
            if(srcShadowSize < shadowSize){
                for(unsigned i = 0; i < shadowSize - srcShadowSize; ++i){
                    *(data + i) = 0xff;
                }

                unsigned j = 0;
                for(unsigned i = shadowSize - srcShadowSize; i < shadowSize; ++i, ++j){
                    *(data + i) = *(srcStatusContent + j);
                }
            }
            else{
                uint8_t* currStatusContent = srcStatusContent + (srcShadowSize - shadowSize);
                for(unsigned i = 0; i < shadowSize; ++i){
                    *(data + i) = *(currStatusContent + i);
                }
            }

            registerFile.setAsInitialized(*iter, data);
            free(data);
        }
    }

    propagatePendingReads(srcRegs, dstRegs);
}