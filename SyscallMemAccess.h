#include "pin.H"
#include "MemoryAccess.h"

class SyscallMemAccess{
    private:
        ADDRINT accessAddress;
        UINT32 accessSize;
        AccessType type;
    
    public:
        SyscallMemAccess(ADDRINT addr, UINT32 size, AccessType type) :
            accessAddress(addr),
            accessSize(size),
            type(type)
        {}

        ADDRINT getAddress() const{
            return accessAddress;
        }

        UINT32 getSize() const{
            return accessSize;
        }

        AccessType getType() const{
            return type;
        }

        bool operator<(const SyscallMemAccess& other) const{
            return this->accessAddress < other.accessAddress;
        }
};