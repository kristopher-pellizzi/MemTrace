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

// Add the offset to the bit word (i.e. shift the whole bitmask by |offset| bits to the left)
static uint8_t* addOffset(uint8_t* data, unsigned offset, unsigned* srcShadowSize, unsigned srcByteSize){
    uint8_t* ret = data;
    unsigned origShadowSize = *srcShadowSize;

    if(srcByteSize % 8 == 0 || offset + srcByteSize % 8 > 8){
        ++(*srcShadowSize);
        ret = (uint8_t*) malloc(sizeof(uint8_t) * (*srcShadowSize));
    }

    unsigned i = 0;
    unsigned j = 0;
    // If we had to allocate a new byte, we need to store the 8 - offset most significant bytes in the least significant 
    // bytes of the newly created byte
    if(ret != data){
        *ret = (uint8_t) 0 | (*data >> (8 - offset));
        ++i;
    }

    for(; i < *srcShadowSize; ++i){
        *(ret + i) = *(data + j++) << offset;
        if(j < origShadowSize)
            *(ret + i++) |= *(data + j) >> (8 - offset);
    }

    return ret;

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

    if(srcByteSize != ma.getSize()){
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

        if(offset != 0){
            data = addOffset(data, offset, &srcShadowSize, srcByteSize);
        }
        
        // Data from registers has size higher or equal than the written memory => 
        // write just the lowest {ma.getSize()} bytes.
        if(srcByteSize >= ma.getSize()){
            // Set to 0 all the bytes that are not actually part of the register status (e.g. for eax, set to 0 the 4 most significant bits)
            // as they are related to rax)
            *data &= ((uint8_t) 0xff >> (8 - srcByteSize % 8));
            uint8_t* src = data;
            src += srcShadowSize - shadowSize;
            set_as_initialized(ma.getAddress(), ma.getSize(), src);
        }
        // Data from registers has a lower size then the written memory => 
        // consider a zero or sign expansion of the value, so expand data as the highest bytes were initialized
        // and store the expanded data
        else{
            uint8_t* expandedData = expandData(data, shadowSize, srcShadowSize);
            set_as_initialized(ma.getAddress(), ma.getSize(), expandedData);
            free(expandedData);
        }

        // If data is still |srcStatusPtr|, it will be automatically freed when the RegsStatus object is destroyed
        if(data != srcStatusPtr)
            free(data);
    }

    warningOpcodes.close();
}