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

    public:
        MemoryAccess(ADDRINT ip, ADDRINT addr, UINT32 size, AccessType type, std::string disasm) : 
            instructionPointer(ip),
            accessAddress(addr),
            accessSize(size),
            type(type),
            instructionDisasm(disasm)
            {};

        ADDRINT getIP() const;

        ADDRINT getAddress() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool operator< (const MemoryAccess &other) const;
};