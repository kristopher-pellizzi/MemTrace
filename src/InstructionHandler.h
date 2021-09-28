#include <map>
#include "pin.H"
#include "Instructions.h"

using std::map;

#ifndef INSTRUCTIONHANDLER
#define INSTRUCTIONHANDLER

class InstructionHandler{
    private:
        map<OPCODE, InstructionEmulator*> emulators;
        InstructionEmulator* defaultLoad;
        InstructionEmulator* defaultStore;

        InstructionHandler();
        void init();

    public:
        // Delete copy constructor and assignment operator
        InstructionHandler(const InstructionHandler& other) = delete;
        void operator=(const InstructionHandler& other) = delete;

        static InstructionHandler& getInstance();
        void handle(OPCODE op, MemoryAccess& ma, set<REG>* regs);

};

#endif //INSTRUCTIONHANDLER