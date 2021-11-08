#include <list>
#include <set>
#include "pin.H"

#ifndef INSTRUCTIONCLASSIFICATION
#define INSTRUCTIONCLASSIFICATION

using std::set;
using std::list;

// This set contains all the opcodes of instructions belonging to any SSE instruction extension ISA.
// This is not created manually, but the tool automatically retrieves the ISA extension an instruction belongs to
// and if it is an SSE instructions, it is added to the set, thus making this information available for any
// module including this header file.
extern set<OPCODE> sseInstructions;

bool isSSEInstruction(INT32 extension);

bool isSSEInstruction(OPCODE opcode);

bool isAutoMov(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs);

bool isPushInstruction(OPCODE opcode);

bool isPopInstruction(OPCODE opcode);

bool isXorInstruction(OPCODE opcode);

bool isEndbrInstruction(OPCODE opcode);

bool isCallInstruction(OPCODE opcode);

bool isSyscallInstruction(OPCODE opcode);

bool isFpuPushInstruction(OPCODE opcode);

bool isFpuPopInstruction(OPCODE opcode);

bool isXsaveInstruction(OPCODE opcode);

bool isXrstorInstruction(OPCODE opcode);

bool shouldLeavePending(OPCODE opcode);

bool isMovInstruction(OPCODE opcode);

bool isCmpInstruction(OPCODE opcode);

#endif //INSTRUCTIONCLASSIFICATION