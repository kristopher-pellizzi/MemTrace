#include "DefaultPropagateInstruction.h"

void DefaultPropagateInstruction::operator() (OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs){
    // There's nothing to propagate
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    RegsStatus srcStatus = getSrcRegsStatus(srcRegs);

    uint8_t* srcStatusContent = srcStatus.getStatus();
    unsigned srcByteSize = srcStatus.getByteSize();
    unsigned srcShadowSize = srcStatus.getShadowSize();

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    std::ofstream warningOpcodes;

    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        unsigned byteSize = registerFile.getByteSize(*iter);
        unsigned shadowSize = registerFile.getShadowRegister(*iter);
        uint8_t* data = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);

        if(srcByteSize != byteSize){
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

            warningOpcodes.open("warningOpcodes.log", std::ios::app);
            warningOpcodes 
                << LEVEL_CORE::OPCODE_StringShort(opcode) 
                << " raised a warning 'cause source register size is "
                << (srcByteSize < byteSize ? "LOWER" : "HIGHER")
                << " than destination register size" << endl;
        }


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
    free(srcStatusContent);
}