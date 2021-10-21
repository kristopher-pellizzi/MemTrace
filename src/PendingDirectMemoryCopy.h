#include "pin.H"
#include "MemoryAccess.h"

#ifndef PENDINGDIRECTMEMORYCOPY
#define PENDINGDIRECTMEMORYCOPY

/*
    Class thought to handle some very special instructions directly reading and writing into
    memory.
    Indeed, if an instruction is a load instruction which does not directly use the read value
    (e.g. MOVSD copies from a memory location M1 to a memory location M2), we first need to retrieve the 
    MemoryAccess object of the uninitialized read, store it somewhere safe and make the next write access
    propagate information from M1 to M2.
*/
class PendingDirectMemoryCopy{
    private:
        MemoryAccess ma;
        bool valid;

    public:
        PendingDirectMemoryCopy();
        PendingDirectMemoryCopy(MemoryAccess& ma);

        ADDRINT getIp();
        MemoryAccess& getAccess();
        bool isValid();
        void setAsInvalid();
};

#endif //PENDINGDIRECTMEMORYCOPY