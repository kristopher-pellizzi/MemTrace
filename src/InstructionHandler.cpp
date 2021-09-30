#include "InstructionHandler.h"
#include "MemoryAccess.h"

InstructionHandler::InstructionHandler(){
    init();
}

void InstructionHandler::init(){
    defaultLoad = new DefaultLoadInstruction();
    defaultRegPropagate = NULL;
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
        defaultLoad->operator()(ma, srcRegs, dstRegs);
    }
    else{
        // TODO: implement default store emulator
        //defaultStore->operator()(ma, dstRegs, srcRegs);
    }
}

void InstructionHandler::handle(OPCODE op, set<REG>* srcRegs, set<REG>* dstRegs){
    auto instr = regEmulators.find(op);

    if(instr != regEmulators.end()){
        instr->second->operator()(srcRegs, dstRegs);
    }
    else{
        defaultRegPropagate->operator()(srcRegs, dstRegs);
    }
}