#include "InstructionHandler.h"
#include "MemoryAccess.h"

InstructionHandler::InstructionHandler(){
    init();
}

void InstructionHandler::init(){
    defaultLoad = new DefaultLoadInstruction();
    defaultRegPropagate = new DefaultPropagateInstruction();
    defaultStore = NULL;    
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
        // This is a store instruction. If there are no source registers, there's nothing to do.
        if(srcRegs == NULL)
            return;

        // TODO: implement default store emulator
        //defaultStore->operator()(ma, dstRegs, srcRegs);
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