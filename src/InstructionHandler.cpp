#include "InstructionHandler.h"
#include "MemoryAccess.h"

InstructionHandler::InstructionHandler(){
    init();
}

void InstructionHandler::init(){
    defaultLoad = new DefaultLoadInstruction();    
}

InstructionHandler& InstructionHandler::getInstance(){
    static InstructionHandler instance;

    return instance;
}

void InstructionHandler::handle(OPCODE op, MemoryAccess& ma, set<REG>* regs){
    auto instr = emulators.find(op);

    if(instr != emulators.end()){
        instr->second->operator()(ma, regs);
    }
    else if(ma.getType() == AccessType::READ){
        defaultLoad->operator()(ma, regs);
    }
    else{
        // TODO: implement default store emulator
        //defaultStore->operator()(ma, regs);
    }
}