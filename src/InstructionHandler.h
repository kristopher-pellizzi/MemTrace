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
        ~InstructionHandler();
        void init();

    public:
        // Delete copy constructor and assignment operator
        InstructionHandler(const InstructionHandler& other) = delete;
        void operator=(const InstructionHandler& other) = delete;

        static InstructionHandler& getInstance();

        void handle(OPCODE op, MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs);
        void handle(OPCODE op, list<REG>* srcRegs, list<REG>* dstRegs);
        // This overloading method is thought to handle situations where a set of registers is simply to be 
        // set as initialized, so it updates the corresponding status and removes any possible pending uninitialized
        // read on those registers
        void handle(list<REG>* initializedRegs);
        // This overloading method is thought to handle store instructions that simply initialize the whole
        // memory location
        void handle(const AccessIndex& ai);
        // This overloading method is thought to handle the direct memory copy instructions (e.g. movsd)
        void handle(MemoryAccess& srcMA, MemoryAccess& dstMA, list<REG>* srcRegs);
};

#endif //INSTRUCTIONHANDLER