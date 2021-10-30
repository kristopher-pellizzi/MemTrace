#include <list>
#include "pin.H"

#ifndef INSTRUCTIONCLASSIFICATION
#define INSTRUCTIONCLASSIFICATION

using std::list;


bool isAutoMov(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs);

bool isPushInstruction(OPCODE opcode);

bool isPopInstruction(OPCODE opcode);

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