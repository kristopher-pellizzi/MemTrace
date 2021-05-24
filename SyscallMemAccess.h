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
            if(this->accessAddress != other.accessAddress)
                return this->accessAddress < other.accessAddress;
            if(this->accessSize != other.accessSize)
                return this->accessSize < other.accessSize;
            if(this->type != other.type){
                return this->type == AccessType::READ;
            }
            return false;
        }
};