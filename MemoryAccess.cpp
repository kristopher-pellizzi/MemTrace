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
    if(accessSize != other.accessSize)
        return accessSize > other.accessSize;
    if(instructionPointer != other.instructionPointer)
        return instructionPointer < other.instructionPointer;
    if(actualInstructionPointer != other.actualInstructionPointer)
        return actualInstructionPointer < other.actualInstructionPointer;
    if(type != other.type)
        return type == AccessType::WRITE;
    if(isUninitializedRead && other.isUninitializedRead){
        if(uninitializedInterval.first != other.uninitializedInterval.first)
            return uninitializedInterval.first < other.uninitializedInterval.first;
        if(uninitializedInterval.second != other.uninitializedInterval.second)
            return uninitializedInterval.second < other.uninitializedInterval.second;
    }
    return false;
}

bool MemoryAccess::operator==(const MemoryAccess& other) const{
    return  
        this->instructionPointer == other.instructionPointer &&
        this->actualInstructionPointer ==  other.actualInstructionPointer &&
        this->accessAddress == other.accessAddress &&
        this->accessSize == other.accessSize &&
        this->type == other.type &&
        this->isUninitializedRead == other.isUninitializedRead &&
        this->uninitializedInterval.first == other.uninitializedInterval.first &&
        this->uninitializedInterval.second == other.uninitializedInterval.second;
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

std::pair<int, int> PartialOverlapAccess::getUninitializedInterval() const{
    return ma.getUninitializedInterval();
}