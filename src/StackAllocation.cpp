#include "StackAllocation.h"

StackAllocation::StackAllocation(){}

StackAllocation::StackAllocation(ADDRINT startAddr, UINT64 size, bool requiresProbe) :
    startAddr(startAddr),
    size(size),
    requiresProbeFlag(requiresProbe)
{}

ADDRINT StackAllocation::getStartAddr() const{
    return startAddr;
}

UINT64 StackAllocation::getSize() const{
    return size;
}

bool StackAllocation::requiresProbe() const{
    return requiresProbeFlag;
}

void StackAllocation::unsetRequiresProbeFlag(){
    requiresProbeFlag = false;
}