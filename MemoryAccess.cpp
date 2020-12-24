#include "MemoryAccess.h"

ADDRINT MemoryAccess::getIP() const{
    return instructionPointer;
}

ADDRINT MemoryAccess::getAddress() const{
    return accessAddress;
}

UINT32 MemoryAccess::getSize() const{
    return accessSize;
}

AccessType MemoryAccess::getType() const{
    return type;
}

std::string MemoryAccess::getDisasm() const{
    return instructionDisasm;
}

bool MemoryAccess::operator<(const MemoryAccess &other) const{
    return 
        accessAddress < other.accessAddress ||
        instructionPointer < other.instructionPointer || 
        accessSize < other.accessSize ||
        type != other.type;
}