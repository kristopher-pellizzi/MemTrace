#include "InstructionHandler.h"
#include "MemoryAccess.h"

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