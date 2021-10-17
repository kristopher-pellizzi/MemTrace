#include <set>

#include "pin.H"

#ifndef SETOPS
#define SETOPS
using std::set;

/*
    Performs set difference operation |s| \ |toRemove|.

    @param s: Set to which the difference operator is applied
    @param toRemove: Set whose elements are removed from |s|, if present
*/
set<REG>* setDiff(set<REG>* s, set<REG>* toRemove);

#endif //SETOPS