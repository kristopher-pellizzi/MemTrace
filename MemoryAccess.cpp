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

uint8_t* MemoryAccess::getUninitializedInterval() const{
    return uninitializedInterval;
}

void MemoryAccess::setUninitializedInterval(uint8_t* interval){
    uninitializedInterval = interval;
}

void MemoryAccess::setUninitializedRead(){
    isUninitializedRead = true;
}

bool MemoryAccess::isStackAccess() const{
    return shadowMemory == stack.getPtr();
}

set<std::pair<unsigned, unsigned>> MemoryAccess::computeIntervals() const{
    return shadowMemory->computeIntervals(uninitializedInterval, accessAddress, accessSize);
}

std::string MemoryAccess::toString() const{
    std::stringstream ss;
    ss <<   "0x" << std::hex << getIP() <<
            "(0x" << getActualIP() << "): " <<
            getDisasm() << " " <<
            (getType() == AccessType::READ ? "R " : "W ") <<
            std::dec << getSize() << " B @ " << std::hex <<
            "0x" << getAddress();
    std::string s = ss.str();
    return s;
}

bool MemoryAccess::operator<(const MemoryAccess &other) const{
    if(executionOrder != other.executionOrder)
        return executionOrder < other.executionOrder;
    return false;
}

bool MemoryAccess::operator==(const MemoryAccess& other) const{
    return this->executionOrder == other.executionOrder;
}

// Returns true if the memory accesses access the very same memory area. 
// NOTE: it does not consider execution order
bool MemoryAccess::compare(const MemoryAccess& other) const{
    UINT32 offset = accessSize % 8;
    UINT32 size = accessSize + offset;
    UINT32 shadowSize = (size % 8 != 0 ? (size / 8) + 1 : (size / 8));

    bool uninitializedReadEq = this->isUninitializedRead == other.isUninitializedRead;
    bool notNullPtr = uninitializedReadEq && this->isUninitializedRead;

    bool ret =  
        this->instructionPointer == other.instructionPointer &&
        this->actualInstructionPointer ==  other.actualInstructionPointer &&
        this->accessAddress == other.accessAddress &&
        this->accessSize == other.accessSize &&
        this->type == other.type &&
        uninitializedReadEq;

    if(ret && notNullPtr)
        ret = ret && (memcmp(this->uninitializedInterval, other.uninitializedInterval, shadowSize) == 0);

    return ret;
}

bool MemoryAccess::operator!=(const MemoryAccess& other) const{
    return !operator==(other);
}




bool MemoryAccess::ExecutionComparator::operator()(const MemoryAccess& ma1, const MemoryAccess& ma2){
    return ma1.executionOrder < ma2.executionOrder;
}


// Implementation of PartialOverlapAccess methods

void PartialOverlapAccess::flagAsPartial(){
    isPartialOverlap = true;
}

const MemoryAccess& PartialOverlapAccess::getAccess() const{
    return ma;
}

bool PartialOverlapAccess::getIsPartialOverlap() const{
    return isPartialOverlap;
}

set<PartialOverlapAccess> PartialOverlapAccess::convertToPartialOverlaps(set<MemoryAccess>& s, bool arePartialOverlaps){
    set<PartialOverlapAccess> ret;
    for(const MemoryAccess& ma : s){
        ret.insert(PartialOverlapAccess(ma, arePartialOverlaps));
    }
    return ret;
}

set<PartialOverlapAccess> PartialOverlapAccess::convertToPartialOverlaps(set<MemoryAccess>& s){
    return convertToPartialOverlaps(s, false);
}

void PartialOverlapAccess::addToSet(set<PartialOverlapAccess>& ps, set<MemoryAccess>& s, bool arePartialOverlaps){
    set<PartialOverlapAccess> toAdd = convertToPartialOverlaps(s, arePartialOverlaps);
    ps.insert(toAdd.begin(), toAdd.end());
}

void PartialOverlapAccess::addToSet(set<PartialOverlapAccess>& ps, set<MemoryAccess>& s){
    set<PartialOverlapAccess> toAdd = convertToPartialOverlaps(s);
    ps.insert(toAdd.begin(), toAdd.end());
}

bool PartialOverlapAccess::operator<(const PartialOverlapAccess& other) const{
    MemoryAccess::ExecutionComparator comp;
    return comp(this->ma, other.ma);
}

// Contained MemoryAccess structure delegation methods

ADDRINT PartialOverlapAccess::getIP() const{
    return ma.getIP();
}

ADDRINT PartialOverlapAccess::getActualIP() const{
    return ma.getActualIP();
}

ADDRINT PartialOverlapAccess::getAddress() const{
    return ma.getAddress();
}

long long int PartialOverlapAccess::getSPOffset() const{
    return ma.getSPOffset();
}

long long int PartialOverlapAccess::getBPOffset() const{
    return ma.getBPOffset();
}

UINT32 PartialOverlapAccess::getSize() const{
    return ma.getSize();
}

AccessType PartialOverlapAccess::getType() const{
    return ma.getType();
}

std::string PartialOverlapAccess::getDisasm() const{
    return ma.getDisasm();
}

bool PartialOverlapAccess::getIsUninitializedRead() const{
    return ma.getIsUninitializedRead();
}

uint8_t* PartialOverlapAccess::getUninitializedInterval() const{
    return ma.getUninitializedInterval();
}

bool PartialOverlapAccess::isStackAccess() const{
    return ma.isStackAccess();
}

set<std::pair<unsigned, unsigned>> PartialOverlapAccess::computeIntervals() const{
    return ma.computeIntervals();
}
