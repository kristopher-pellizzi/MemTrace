#include <list>
#include <map>
#include "pin.H"
#include "../TagManager.h"
#include "../AccessIndex.h"
#include "../MemoryAccess.h"
#include "../ShadowRegisterFile.h"

using std::pair;
using std::list;
using std::map;

#ifndef PENDINGREADS
#define PENDINGREADS

typedef pair<ADDRINT, ADDRINT> range_t;

class IncreasingStartRangeSorter{
    public:
        bool operator()(const range_t& x, const range_t& y) const;
};

class IncreasingEndRangeSorter{
    public:
        bool operator()(const range_t& x, const range_t& y) const;
};

extern map<unsigned, set<tag_t>> pendingUninitializedReads;
extern map<pair<ADDRINT, ADDRINT>, set<tag_t>, IncreasingStartRangeSorter> storedPendingUninitializedReads;
 
void addPendingRead(list<REG>* regs, const MemoryAccess& ma);
void addPendingRead(list<REG>* dstRegs, set<tag_t>& tags);
void updatePendingReads(list<REG>* dstRegs);

void propagatePendingReads(list<REG>* srcRegs, list<REG>* dstRegs);

void updateStoredPendingReads(const AccessIndex& ai);
void storePendingReads(list<REG>* srcRegs, MemoryAccess& ma);
map<range_t, set<tag_t>> getStoredPendingReads(MemoryAccess& ma);
map<range_t, set<tag_t>> getStoredPendingReads(AccessIndex& ai);
void copyStoredPendingReads(MemoryAccess& srcMA, MemoryAccess& dstMA, list<REG>* srcRegs);

/*
    Compute the difference |ranges| - |r2|, which means that we remove from range
    |ranges| the whole range represented by |r2|.
    Example: ranges = {[0, 32]}; r2 = [4, 12] => result = {[0, 3], [13, 32]}
*/
set<range_t, IncreasingStartRangeSorter>& rangeDiff(set<range_t, IncreasingStartRangeSorter>& ranges, const range_t& r2);

/*
    Compute the intersection between |ranges| and |r2|, which means that we remove from
    |ranges| all the ranges not belonging to r2.
    Example: ranges = {[0, 6], [10, 32]}; r2 = [4, 12] => result = {[4, 6], [10, 32]}
*/
set<range_t, IncreasingStartRangeSorter>& rangeIntersect(set<range_t, IncreasingStartRangeSorter>& ranges, set<range_t, IncreasingStartRangeSorter>& intersectSet);

#endif //PENDINGREADS