#include "DefaultStoreInstruction.h"

using std::ofstream;

static uint8_t* expandData(uint8_t* data, unsigned shadowSize, unsigned regShadowSize){
    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);
    unsigned diff = shadowSize - regShadowSize;

    for(unsigned i = 0; i < diff; ++i){
        *(ret + i) = 0xff;
    }

    unsigned j = 0;
    for(unsigned i = diff; i < shadowSize; ++i, ++j){
        *(ret + i) = *(data + j);
    }

    return ret;
}

DefaultStoreInstruction::DefaultStoreInstruction(){
    initVerifiedInstructions();
}

void DefaultStoreInstruction::initVerifiedInstructions(){
    verifiedInstructions.insert(XED_ICLASS_MOVD);
    verifiedInstructions.insert(XED_ICLASS_MOVQ);
    verifiedInstructions.insert(XED_ICLASS_VMOVD);
    verifiedInstructions.insert(XED_ICLASS_VMOVQ);
}

void  DefaultStoreInstruction::operator() (MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs){
    // If there are no source registers, probably an immediate is going to be stored, so just set everything as initialized
    if(srcRegs == NULL){
        set_as_initialized(ma.getAddress(), ma.getSize());
        return;
    }

    ofstream warningOpcodes;

    // Get the content status of all source registers and merge it with status loaded from memory (bitwise AND)
    RegsStatus srcStatus = getSrcRegsStatus(srcRegs);
    uint8_t* srcStatusPtr = srcStatus.getStatus();
    unsigned srcByteSize = srcStatus.getByteSize();
    unsigned srcShadowSize = srcStatus.getShadowSize();
    bool isVerifiedInstruction = verifiedInstructions.find(ma.getOpcode()) != verifiedInstructions.end();

    if(!isVerifiedInstruction && srcByteSize != ma.getSize()){
        #ifdef DEBUG
        cerr 
            << "[DefaultStoreInstruction] Warning: reading source registers." << endl;
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

    // Set the corresponding shadow memory as completely initialized
    if(srcStatus.isAllInitialized()){  
        set_as_initialized(ma.getAddress(), ma.getSize());
    }
    // Set the corresponding shadow memory to reflect status of src registers
    else{
        unsigned shadowSize = ma.getSize() + ma.getAddress() % 8;
        unsigned offset = ma.getAddress() % 8;
        shadowSize = shadowSize % 8 != 0 ? (shadowSize / 8) + 1 : shadowSize / 8;
        uint8_t* data = srcStatusPtr; 
        
        // Data from registers has size higher or equal than the written memory => 
        // write just the lowest {ma.getSize()} bytes.
        if(srcByteSize >= ma.getSize()){
            // Set to 0 all the bytes that are not actually part of the register status (e.g. for eax, set to 0 the 4 most significant bits)
            // as they are related to rax)
            if(srcByteSize % 8 != 0)
                *data &= ((uint8_t) 0xff >> (8 - srcByteSize % 8));

            if(offset != 0){
                data = addOffset(data, offset, &srcShadowSize, srcByteSize);
            }
            uint8_t* src = data;
            src += srcShadowSize - shadowSize;
            set_as_initialized(ma.getAddress(), ma.getSize(), src);
        }
        // Data from registers has a lower size then the written memory => 
        // consider a zero or sign expansion of the value, so expand data as the highest bytes were initialized
        // and store the expanded data
        else{
            if(offset != 0){
                data = addOffset(data, offset, &srcShadowSize, srcByteSize);
            }
            uint8_t* expandedData = expandData(data, shadowSize, srcShadowSize);
            set_as_initialized(ma.getAddress(), ma.getSize(), expandedData);
            free(expandedData);
        }

        // If data is still |srcStatusPtr|, it will be automatically freed when the RegsStatus object is destroyed
        if(data != srcStatusPtr)
            free(data);
    }

    warningOpcodes.close();

    storePendingReads(srcRegs, ma);
}