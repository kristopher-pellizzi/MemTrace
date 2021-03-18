#include "InitializedMemory.h"

std::pair<set<AccessIndex>::iterator, bool> InitializedMemory::insert(const AccessIndex& toInsert){
    InitializedMemory* currentFrame = this;
    while(toInsert.getFirst() > currentFrame->frameBeginning && currentFrame->prev != NULL){
        currentFrame = currentFrame->prev;
    }
    return currentFrame->initializations.insert(toInsert);
}

void InitializedMemory::insert(set<AccessIndex>::iterator firstIterator, set<AccessIndex>::iterator lastIterator){
    InitializedMemory* currentFrame = this;
    while(firstIterator->getFirst() > currentFrame->frameBeginning && currentFrame->prev != NULL){
        currentFrame = currentFrame->prev;
    }
    currentFrame->initializations.insert(firstIterator, lastIterator);
}

set<AccessIndex>::const_iterator InitializedMemory::find(const AccessIndex& val) const{
    const InitializedMemory* currentFrame = this;
    set<AccessIndex>::const_iterator ret = currentFrame->initializations.find(val);
    while(ret == currentFrame->end() && currentFrame->prev != NULL){
        currentFrame = currentFrame->prev;
        ret = currentFrame->initializations.find(val);
    }
    if(ret == currentFrame->end())
        return this->end();
    return ret;
}

set<AccessIndex>::const_iterator InitializedMemory::begin() const{
    return initializations.begin();
}

set<AccessIndex>::const_iterator InitializedMemory::end() const{
    return initializations.end();
}

InitializedMemory* InitializedMemory::deleteFrame(){
    InitializedMemory* ret = this->prev;
    delete this;
    return ret;
}

std::pair<int, int> InitializedMemory::getUninitializedInterval(AccessIndex& ai){
        
    // If there are no accesses with an address lower than the highest address of ai,
    // this access can't be initialized.
    if(this->begin()->getFirst() >= ai.getFirst() + ai.getSecond())
        return std::pair<int, int>(0, ai.getSecond() - 1);

    // If there's a write access to the same address, with the same size
    // then some instruction already wrote something in the very same
    // memory area we are reading from
    if(this->find(ai) != this->end()){
        // It is an initialized memory access
        return std::pair<int, int>(-1, -1);
    }
    
    std::pair<unsigned int, unsigned int> uninitializedBytes(0, ai.getSecond());

    ADDRINT targetStart = ai.getFirst();
    ADDRINT targetEnd = targetStart + ai.getSecond() - 1;

    InitializedMemory* currentFrame = this;
    do {
        set<AccessIndex>::const_iterator iter = currentFrame->begin();
        
        while(iter != currentFrame->end()){
            ADDRINT currentStart = iter->getFirst();
            ADDRINT currentEnd = currentStart + iter->getSecond() - 1;
            // If will remain uninitialized bytes at the beginning, between iter start and target start
            // NOTE: it may be possible that there's an uninitialized "hole" in the target ai
            // Those situations are considered as if from the hole to the end of the target ai
            // the access is not initialized
            if(currentStart > targetStart + uninitializedBytes.first){
                // The access is not initialized
                return std::pair<int, int>(uninitializedBytes.first, uninitializedBytes.second - 1);
            }
            // If there are no more uninitialized bytes
            if(targetEnd <= currentEnd){
                // The access is initialized
                return std::pair<int, int>(-1, -1);
            }

            if(currentEnd >= targetStart){
                int initializedPortion = iter->getSecond() - (targetStart + uninitializedBytes.first - currentStart);
                // If initializedPortion is negative, then it means that another write access completely initialized the
                // same bytes that this access has initialized, so there's no need to update uninitializedBytes.first,
                // as it already takes into account the bytes initialized by the considered write access
                if(initializedPortion > 0)
                    uninitializedBytes.first += initializedPortion;
            }
            ++iter;   
        }
        currentFrame = currentFrame->prev;
    } while(currentFrame != NULL && currentFrame->begin()->getFirst() <= targetEnd);
    // There are no more AccessIndexes in the structure, and there are still uninitialized bytes
    return std::pair<int, int>(uninitializedBytes.first, uninitializedBytes.second - 1);
}

