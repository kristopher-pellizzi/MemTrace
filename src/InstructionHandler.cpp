#include "InstructionHandler.h"
#include "MemoryAccess.h"
#include "misc/PendingReads.h"

InstructionHandler::InstructionHandler(){
    init();
}

InstructionHandler::~InstructionHandler(){
    delete(defaultLoad);
    delete(defaultRegPropagate);
    delete(defaultStore);

    // Delete emulator for CWD/CDQ/CQO
    delete(regEmulators[XED_ICLASS_CWD]);

    // Delete emulator for PMOVMSKB/VPMOVMSKB
    delete(regEmulators[XED_ICLASS_PMOVMSKB]);

    // Delete emulator for VPBROADCASTB/VPBROADCASTW/VPBROADCASTD/VPBROADCASTQ
    delete(regEmulators[XED_ICLASS_VPBROADCASTB]);
    delete(memEmulators[XED_ICLASS_VPBROADCASTB]);

    // Delete emulator for FST/FSTP
    delete(regEmulators[XED_ICLASS_FST]);
    delete(memEmulators[XED_ICLASS_FST]);

    // Delete emulator for XSAVE/XSAVEC/XSAVEOPT/XSAVES/FXSAVE
    delete(memEmulators[XED_ICLASS_XSAVE]);
}

void InstructionHandler::init(){
    defaultLoad = new DefaultLoadInstruction();
    defaultRegPropagate = new DefaultPropagateInstruction();
    defaultStore = new DefaultStoreInstruction();    

    RegInstructionEmulator* convertInstructionEmulator = new ConvertInstruction();
    regEmulators[XED_ICLASS_CWD] = convertInstructionEmulator;
    regEmulators[XED_ICLASS_CDQ] = convertInstructionEmulator;
    regEmulators[XED_ICLASS_CQO] = convertInstructionEmulator;

    RegInstructionEmulator* PMOVMSKBEmulator = new PmovmskbInstruction();
    regEmulators[XED_ICLASS_PMOVMSKB] = PMOVMSKBEmulator;
    regEmulators[XED_ICLASS_VPMOVMSKB] = PMOVMSKBEmulator;

    RegInstructionEmulator* VpbroadcastEmulator = new VpbroadcastInstruction();
    regEmulators[XED_ICLASS_VPBROADCASTB] = VpbroadcastEmulator;
    regEmulators[XED_ICLASS_VPBROADCASTW] = VpbroadcastEmulator;
    regEmulators[XED_ICLASS_VPBROADCASTD] = VpbroadcastEmulator;
    regEmulators[XED_ICLASS_VPBROADCASTQ] = VpbroadcastEmulator;

    RegInstructionEmulator* fstEmulator = new FstInstruction();
    regEmulators[XED_ICLASS_FST] = fstEmulator;
    regEmulators[XED_ICLASS_FSTP] = fstEmulator;

    MemInstructionEmulator* memVpbroadcastEmulator = new MemVpbroadcastInstruction();
    memEmulators[XED_ICLASS_VPBROADCASTB] = memVpbroadcastEmulator;
    memEmulators[XED_ICLASS_VPBROADCASTW] = memVpbroadcastEmulator;
    memEmulators[XED_ICLASS_VPBROADCASTD] = memVpbroadcastEmulator;
    memEmulators[XED_ICLASS_VPBROADCASTQ] = memVpbroadcastEmulator;

    MemInstructionEmulator* memFstEmulator = new MemFstInstruction();
    memEmulators[XED_ICLASS_FST] = memFstEmulator;
    memEmulators[XED_ICLASS_FSTP] = memFstEmulator;
    memEmulators[XED_ICLASS_FIST] = memFstEmulator;
    memEmulators[XED_ICLASS_FISTP] = memFstEmulator;
    memEmulators[XED_ICLASS_FISTTP] = memFstEmulator;

    MemInstructionEmulator* xsaveEmulator = new XsaveInstruction();
    memEmulators[XED_ICLASS_XSAVE] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVE64] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVEC] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVEC64] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVES] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVES64] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVEOPT] = xsaveEmulator;
    memEmulators[XED_ICLASS_XSAVEOPT64] = xsaveEmulator;
    memEmulators[XED_ICLASS_FXSAVE] = xsaveEmulator;
    memEmulators[XED_ICLASS_FXSAVE64] = xsaveEmulator;

}

InstructionHandler& InstructionHandler::getInstance(){
    static InstructionHandler instance;

    return instance;
}

void InstructionHandler::handle(OPCODE op, MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs){
    auto instr = memEmulators.find(op);

    if(instr != memEmulators.end()){
        instr->second->operator()(ma, srcRegs, dstRegs);
    }
    else if(ma.getType() == AccessType::READ){
        // This is a load instruction. If there are no destination registers, there's nothing to do.
        if(dstRegs == NULL)
            return;

        defaultLoad->operator()(ma, srcRegs, dstRegs);
    }
    else{
        /*
            Note that if srcRegs is NULL, we must set all the memory as initialized, otherwise we may lose information
            about immediate stores
        */
        defaultStore->operator()(ma, srcRegs, dstRegs);
    }
}

void InstructionHandler::handle(OPCODE op, set<REG>* srcRegs, set<REG>* dstRegs){
    if(srcRegs == NULL || dstRegs == NULL){
        return;
    }

    auto instr = regEmulators.find(op);

    if(instr != regEmulators.end()){
        instr->second->operator()(op, srcRegs, dstRegs);
    }
    else{
        defaultRegPropagate->operator()(op, srcRegs, dstRegs);
    }
}

void InstructionHandler::handle(set<REG>* initializedRegs){
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    registerFile.setAsInitialized(initializedRegs);
    updatePendingReads(initializedRegs);
}

void InstructionHandler::handle(const AccessIndex& ai){
    ADDRINT addr = ai.getFirst();
    UINT32 size = ai.getSecond();

    set_as_initialized(addr, size);
    updateStoredPendingReads(ai);
}

void InstructionHandler::handle(MemoryAccess& srcMA, MemoryAccess& dstMA){
    UINT32 srcByteSize = srcMA.getSize();
    UINT32 dstByteSize = dstMA.getSize();
    ADDRINT srcAddr = srcMA.getAddress();
    ADDRINT dstAddr = dstMA.getAddress();

    uint8_t* srcStatus = cutUselessBits(srcMA.getUninitializedInterval(), srcAddr, srcByteSize);

    unsigned dstOffset = dstAddr % 8;

    unsigned srcShadowSize = srcByteSize;
    srcShadowSize = srcShadowSize % 8 != 0 ? (srcShadowSize / 8) + 1 : srcShadowSize / 8;

    unsigned dstShadowSize = dstByteSize + dstOffset;
    dstShadowSize = dstShadowSize % 8 != 0 ? (dstShadowSize / 8) + 1 : dstShadowSize / 8;

    if(srcByteSize % 8 != 0){
        *srcStatus &= (uint8_t) 0xff >> (8 - srcByteSize % 8);
    }

    uint8_t* dstStatus = srcStatus;

    if(dstOffset != 0)
        dstStatus = addOffset(dstStatus, dstOffset, &srcShadowSize, srcByteSize);

    if(srcByteSize >= dstByteSize){
        uint8_t* srcPtr = dstStatus;
        srcPtr += srcShadowSize - dstShadowSize;
        set_as_initialized(dstAddr, dstByteSize, srcPtr);
    }
    else{
        uint8_t* expandedData = (uint8_t*) malloc(sizeof(uint8_t) * dstShadowSize);
        unsigned diff = dstShadowSize - srcShadowSize;
        for(unsigned i = 0; i < diff; ++i){
            *(expandedData + i) = 0xff;
        }

        unsigned j = 0;
        for(unsigned i = diff; i < dstShadowSize; ++i, ++j){
            *(expandedData + i) = *(dstStatus + j);
        }

        set_as_initialized(dstAddr, dstByteSize, expandedData);
        free(expandedData);
    }

    free(srcStatus);
    free(dstStatus);

    copyStoredPendingReads(srcMA, dstMA);
}