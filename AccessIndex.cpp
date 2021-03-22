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