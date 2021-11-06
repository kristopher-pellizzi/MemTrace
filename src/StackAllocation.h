#include "pin.H"

class StackAllocation{
    private:
        ADDRINT startAddr;
        UINT64 size;
        bool requiresProbeFlag;
    
    public:
        StackAllocation();

        StackAllocation(ADDRINT startAddr, UINT64 size, bool requiresProbe);

        ADDRINT getStartAddr() const;
        UINT64 getSize() const;
        bool requiresProbe() const;
        void unsetRequiresProbeFlag();
};