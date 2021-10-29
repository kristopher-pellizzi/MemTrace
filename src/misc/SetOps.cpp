#include "SetOps.h"

list<REG>* setDiff(list<REG>* s, list<REG>* toRemove){
    if(s == NULL || toRemove == NULL)
        return s;
        
    for(auto iter = toRemove->begin(); iter != toRemove->end(); ++iter){
        for(auto sIter = s->begin(); sIter != s->end(); ++sIter){
            if(*sIter == *iter){
                auto toErase = sIter;
                --sIter;
                s->erase(toErase);
            }
        }
    }

    return s;
}