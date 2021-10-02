#include <set>
#include <map>
#include "../AccessIndex.h"
#include "../MemoryAccess.h"
#include "../ShadowRegisterFile.h"

using std::pair;
using std::set;
using std::map;

#ifndef PENDINGREADS
#define PENDINGREADS

extern map<unsigned, set<pair<AccessIndex, MemoryAccess>>> pendingUninitializedReads;
 
void addPendingRead(set<REG>* regs, const AccessIndex& ai, const MemoryAccess& ma);
void propagatePendingReads(set<REG>* srcRegs, set<REG>* dstRegs);
#endif //PENDINGREADS