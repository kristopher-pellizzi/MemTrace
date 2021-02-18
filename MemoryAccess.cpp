#include "MemoryAccess.h"

ADDRINT MemoryAccess::getIP() const{
    return instructionPointer;
}

ADDRINT MemoryAccess::getActualIP() const{
    return actualInstructionPointer;
}

ADDRINT MemoryAccess::getAddress() const{
    return accessAddress;
}

long long int MemoryAccess::getSPOffset() const{
    return spOffset;
}

long long int MemoryAccess::getBPOffset() const{
    return bpOffset;
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

bool MemoryAccess::getIsUninitializedRead() const{
    return isUninitializedRead;
}

std::pair<int, int> MemoryAccess::getUninitializedInterval() const{
    return uninitializedInterval;
}

void MemoryAccess::setUninitializedInterval(std::pair<int, int>& interval){
    uninitializedInterval.first = interval.first;
    uninitializedInterval.second = interval.second;
}

void MemoryAccess::setUninitializedRead(){
    isUninitializedRead = true;
}

bool MemoryAccess::operator<(const MemoryAccess &other) const{
    if(accessAddress != other.accessAddress)
        return accessAddress < other.accessAddress;
    if(instructionPointer != other.instructionPointer)
        return instructionPointer < other.instructionPointer;
    if(accessSize != other.accessSize)
        return accessSize < other.accessSize;
    if(type != other.type)
        return type == AccessType::WRITE;
    return false;
}