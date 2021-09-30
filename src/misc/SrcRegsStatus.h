#include <set>
#include "pin.H"
#include "../ShadowRegisterFile.h"

using std::pair;
using std::set;

pair<uint8_t*, unsigned> getSrcRegsStatus(set<REG>* srcRegs);