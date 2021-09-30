#include <map>
#include "pin.H"

#include "RegInstructionEmulator.h"
#include "MemInstructionEmulator.h"
#include "Instructions.h"

using std::map;

#ifndef INSTRUCTIONHANDLER
#define INSTRUCTIONHANDLER

class InstructionHandler{
    private:
        map<OPCODE, MemInstructionEmulator*> memEmulators;
        map<OPCODE, RegInstructionEmulator*> regEmulators;
        MemInstructionEmulator* defaultLoad;
        MemInstructionEmulator* defaultStore;
        RegInstructionEmulator* defaultRegPropagate;

        InstructionHandler();
        void init();

    public:
        // Delete copy constructor and assignment operator
        InstructionHandler(const InstructionHandler& other) = delete;
        void operator=(const InstructionHandler& other) = delete;

        static InstructionHandler& getInstance();
        void handle(OPCODE op, MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs);
        void handle(OPCODE op, set<REG>* srcRegs, set<REG>* dstRegs);

};

#endif //INSTRUCTIONHANDLER