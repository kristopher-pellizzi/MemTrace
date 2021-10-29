#include <list>

#include "pin.H"

#ifndef SETOPS
#define SETOPS
using std::list;

/*
    Performs set difference operation |s| \ |toRemove|.

    @param s: Set to which the difference operator is applied
    @param toRemove: Set whose elements are removed from |s|, if present
*/
list<REG>* setDiff(list<REG>* s, list<REG>* toRemove);

#endif //SETOPS