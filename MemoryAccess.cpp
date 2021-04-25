#include "MemoryAccess.h"
#include <iostream>

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

// This takes the shadow memory dump saved in the MemoryAccess object and computes the set of uninitialized
// intervals from that by scanning its bits. For this reason, this is a quite expensive operation, and the
// more bytes are accessed by a MemoryAccess, the more expensive it is.
// However, this is executed only for those uninitialized read accesses saved by the tool (which shouldn't be so
// many in a well produced program), most of which are usually of a few bytes.
set<std::pair<unsigned, unsigned>> MemoryAccess::computeIntervals() const{
    set<std::pair<unsigned, unsigned>> ret;

    uint8_t* addr = uninitializedInterval;
    unsigned offset = accessAddress % 8;
    UINT32 size = accessSize + offset;
    UINT32 shadowSize = (size % 8 != 0 ? (size / 8) + 1 : (size / 8));
    uint8_t mask = 0xff << (8 - (size % 8));

    while(shadowSize > 0){
        uint8_t val = *addr;
        val |= mask;
        val |= (shadowSize != 1 ? 0 : ((1 << offset) - 1));
        mask = 0;

        if(val != 0xff){
            // Start scanning the bits to find the lower and upper bounds of the interval
            mask = 1 << 7;
            int counter = 0;

            // Starting from the most significant bit, scan every other bit until a 0 is found (guaranteed to be found, 
            // as we already know |val| is not 0xff
            while((val & mask) != 0){
                ++counter;
                mask >>= 1;
            }

            int upperBound = shadowSize * 8 - counter - 1 - offset;

            while((val & mask) == 0 && mask != 0){
                ++counter;
                mask >>= 1;
            }

            // If another 1 is found inside the same shadow byte, we have found the interval's lower bound
            if(mask != 0){
                int lowerBound = shadowSize * 8 - counter - offset;
                ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                // Update mask in order to ignore the interval just found
                mask = 0xff << (8 - counter);
            }
            // the lower bound is in the following shadow byte. Before scanning bit by bit, try to see if the whole
            // shadow byte is 0. If it's not, scan the byte one bit at a time.
            else{
                bool lowerBoundFound = false;

                while(!lowerBoundFound){
                    --shadowSize;
                    ++addr;

                    // This condition evaluates to true if there's no bit set to 1 starting from the |upperBound|.
                    // This means that also the very first byte of the access is uninitialized, so |lowerBound| is 0
                    if(shadowSize == 0){
                        int lowerBound = 0;
                        ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                        break;
                    }

                    if(shadowSize == 1)
                        offset = accessAddress % 8;

                    val = *addr;
                    val |= (shadowSize != 1 ? 0 : ((1 << offset) - 1));

                    if(val != 0){
                        mask = 1 << 7;
                        counter = 0;

                        // Starting from the most significant bit, scan every other bit until a 1 is found (guaranteed to be found, 
                        // as we already know |val| is not 0
                        while((val & mask) == 0){
                            ++counter;
                            mask >>= 1;
                        }

                        int lowerBound = shadowSize * 8 - counter - offset;
                        ret.insert(std::pair<unsigned, unsigned>(lowerBound, upperBound));
                        mask = 0xff << (8 - counter);
                        lowerBoundFound = true;
                    }
                }
            }
        }
        // This shadow byte is 0xff. No uninitialized interval can be found
        else{
            // |addr| is increased only if it was 0xff. This way, it is checked again after computing
            // interval's bounds in case there are more "holes" in the same shadow byte 
            // (e.g. shadowByte = 11011001 => intervals are [1,2] and [5, 5], assuming only 1 byte is accessed by the application)
            --shadowSize;
            ++addr;
        }
    }

    return ret;
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

set<std::pair<unsigned, unsigned>> PartialOverlapAccess::computeIntervals() const{
    return ma.computeIntervals();
}
