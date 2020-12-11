#include "MemoryAccess.h"

ADDRINT MemoryAccess::getIP(){
    return instructionPointer;
}

ADDRINT MemoryAccess::getAddress(){
    return accessAddress;
}

UINT32 MemoryAccess::getSize(){
    return accessSize;
}

AccessType MemoryAccess::getType(){
    return type;
}

std::string MemoryAccess::getDisasm(){
    return instructionDisasm;
}