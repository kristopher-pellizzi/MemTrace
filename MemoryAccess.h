#include "pin.H"

enum class AccessType{
    READ,
    WRITE
};

class MemoryAccess{
    private:
        ADDRINT instructionPointer;
        ADDRINT accessAddress;
        UINT32 accessSize;
        AccessType type;
        std::string instructionDisasm;
        bool isUninitializedRead;
        std::pair<int, int> uninitializedInterval;

    public:
        MemoryAccess(ADDRINT ip, ADDRINT addr, UINT32 size, AccessType type, std::string disasm) : 
            instructionPointer(ip),
            accessAddress(addr),
            accessSize(size),
            type(type),
            instructionDisasm(disasm),
            isUninitializedRead(false)
            {
                uninitializedInterval.first = -1;
                uninitializedInterval.second = -1;
            };

        ADDRINT getIP() const;

        ADDRINT getAddress() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool getIsUninitializedRead() const;

        std::pair<int, int> getUninitializedInterval() const;

        void setUninitializedInterval(std::pair<int, int>& interval);

        void setUninitializedRead();

        bool operator< (const MemoryAccess &other) const;
};