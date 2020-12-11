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

        ADDRINT getIP();

        ADDRINT getAddress();

        UINT32 getSize();

        AccessType getType();

        std::string getDisasm();
};