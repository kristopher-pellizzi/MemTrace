#include "SetOps.h"

set<REG>* setDiff(set<REG>* s, set<REG>* toRemove){
    if(s == NULL || toRemove == NULL)
        return s;
        
    for(auto iter = toRemove->begin(); iter != toRemove->end(); ++iter){
        auto found = s->find(*iter);
        if(found != s->end()){
            s->erase(found);
        }
    }

    return s;
}