#include "InitializedMemory.h"

std::pair<set<AccessIndex>::iterator, bool> InitializedMemory::insert(const AccessIndex& toInsert){
    InitializedMemory* currentFrame = this;
    while(toInsert.getFirst() >= currentFrame->frameBeginning && currentFrame->prev != NULL){
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

