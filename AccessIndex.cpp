#include "AccessIndex.h"

ADDRINT AccessIndex::getFirst() const{
    return elements.first;
}

UINT32 AccessIndex::getSecond() const{
    return elements.second;
}

bool AccessIndex::operator<(const AccessIndex& other) const{
    return elements.first < other.elements.first || (elements.first == other.elements.first && elements.second > other.elements.second);
}

bool AccessIndex::operator==(const AccessIndex &other) const{
    return elements.first == other.elements.first && elements.second == other.elements.second;
}

bool AccessIndex::operator!=(const AccessIndex& other) const{
    return !operator==(other);
}



bool AccessIndex::LastAccessedByteSorter::operator()(const AccessIndex& ai1, const AccessIndex& ai2) const{
    return ai1.getFirst() + ai1.getSecond() < ai2.getFirst() + ai2.getSecond();
}