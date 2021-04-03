#include <map>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

#include "pin.H"

using std::map;

extern uint8_t* highestShadowAddr;
extern map<THREADID, ADDRINT> threadInfos;

uint8_t* getShadowAddr(ADDRINT addr);

void set_as_initialized(ADDRINT addr, UINT32 size);

std::pair<int, int> getUninitializedInterval(ADDRINT addr, UINT32 size);

void reset(ADDRINT addr);

void shadowInit();