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

    // Delete emulator for XRSTOR/XRSTORS/FXRSTOR
    delete(memEmulators[XED_ICLASS_XRSTOR]);
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

    MemInstructionEmulator* xrstorEmulator = new XrstorInstruction();
    memEmulators[XED_ICLASS_XRSTOR] = xrstorEmulator;
    memEmulators[XED_ICLASS_XRSTOR64] = xrstorEmulator;
    memEmulators[XED_ICLASS_XRSTORS] = xrstorEmulator;
    memEmulators[XED_ICLASS_XRSTORS64] = xrstorEmulator;
    memEmulators[XED_ICLASS_FXRSTOR] = xrstorEmulator;
    memEmulators[XED_ICLASS_FXRSTOR64] = xrstorEmulator;

}

InstructionHandler& InstructionHandler::getInstance(){
    static InstructionHandler instance;

    return instance;
}

void InstructionHandler::handle(OPCODE op, MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs){
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

void InstructionHandler::handle(OPCODE op, list<REG>* srcRegs, list<REG>* dstRegs){
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

void InstructionHandler::handle(list<REG>* initializedRegs){
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

static uint8_t* andSrcRegsStatus(uint8_t* memStatus, unsigned& byteSize, unsigned& shadowSize, list<REG>* srcRegs){
    RegsStatus srcRegsStatus = getSrcRegsStatus(srcRegs);
    if(srcRegsStatus.isAllInitialized())
        return memStatus;

    uint8_t* regsStatus = srcRegsStatus.getStatus();
    unsigned regsByteSize = srcRegsStatus.getByteSize();
    unsigned regsShadowSize = srcRegsStatus.getShadowSize();
    uint8_t* ret;
    uint8_t* other;
    unsigned retShadowSize;
    unsigned otherShadowSize;

    if(regsByteSize <= byteSize){
        ret = memStatus;
        other = regsStatus;
        retShadowSize = shadowSize;
        otherShadowSize = regsShadowSize;
    }
    else{
        ret = (uint8_t*) malloc(sizeof(regsShadowSize));
        other = memStatus;
        retShadowSize = regsShadowSize;
        otherShadowSize = shadowSize;
        memcpy(ret, regsStatus, regsShadowSize);
    }

    uint8_t* ptr = ret + retShadowSize - 1;
    uint8_t* otherPtr = other + otherShadowSize - 1;

    for(unsigned i = 0; i < shadowSize; ++i, --ptr, --otherPtr){
        *ptr &= *otherPtr;
    }

    if(ret != memStatus){
        byteSize = regsByteSize;
        shadowSize = regsShadowSize;
        free(memStatus);
    }

    return ret;
}

void InstructionHandler::handle(MemoryAccess& srcMA, MemoryAccess& dstMA, list<REG>* srcRegs){
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

    if(srcRegs != NULL)
        srcStatus = andSrcRegsStatus(srcStatus, srcByteSize, srcShadowSize, srcRegs);

    uint8_t* dstStatus = srcStatus;

    if(srcByteSize >= dstByteSize){
        if(srcByteSize % 8 != 0){
            *srcStatus &= (uint8_t) 0xff >> (8 - srcByteSize % 8);
        }

        if(dstOffset != 0)
            dstStatus = addOffset(dstStatus, dstOffset, &srcShadowSize, srcByteSize);

        uint8_t* srcPtr = dstStatus;
        srcPtr += srcShadowSize - dstShadowSize;
        set_as_initialized(dstAddr, dstByteSize, srcPtr);
    }
    else{
        if(dstOffset != 0){
            dstStatus = addOffset(dstStatus, dstOffset, &srcShadowSize, srcByteSize);
        }

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
    if(srcStatus != dstStatus)
        free(dstStatus);

    copyStoredPendingReads(srcMA, dstMA, srcRegs);
}