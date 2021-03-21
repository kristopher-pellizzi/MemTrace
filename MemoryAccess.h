#include "pin.H"
#include <set>

using std::set;

enum class AccessType{
    READ,
    WRITE
};

class MemoryAccess{
    protected:
        unsigned long long executionOrder;

    private:
        ADDRINT instructionPointer;
        ADDRINT actualInstructionPointer;
        ADDRINT accessAddress;
        long long int spOffset;
        long long int bpOffset;
        UINT32 accessSize;
        AccessType type;
        std::string instructionDisasm;
        bool isUninitializedRead;
        std::pair<int, int> uninitializedInterval;

    public:
        MemoryAccess(unsigned long long executionOrder, ADDRINT ip, ADDRINT actualInstructionPointer, ADDRINT addr, int spOffset, int bpOffset, UINT32 size, AccessType type, std::string disasm) : 
            executionOrder(executionOrder),
            instructionPointer(ip),
            actualInstructionPointer(actualInstructionPointer),
            accessAddress(addr),
            spOffset(spOffset),
            bpOffset(bpOffset),
            accessSize(size),
            type(type),
            instructionDisasm(disasm),
            isUninitializedRead(false)
            {
                uninitializedInterval.first = -1;
                uninitializedInterval.second = -1;
            };

        ADDRINT getIP() const;

        ADDRINT getActualIP() const;

        ADDRINT getAddress() const;

        long long int getSPOffset() const;

        long long int getBPOffset() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool getIsUninitializedRead() const;

        std::pair<int, int> getUninitializedInterval() const;

        void setUninitializedInterval(std::pair<int, int>& interval);

        void setUninitializedRead();

        bool operator< (const MemoryAccess &other) const;

        friend struct ExecutionComparator;



        // Define a functor class which allows to order MemoryAccess objects according to their execution order
        struct ExecutionComparator{
            bool operator()(const MemoryAccess& ma1, const MemoryAccess& ma2);
        };
};

class PartialOverlapAccess{
    private:
        MemoryAccess ma;
        bool isPartialOverlap;

        PartialOverlapAccess(MemoryAccess& ma, bool isPartialOverlap) :
            ma(ma),
            isPartialOverlap(isPartialOverlap){}

        PartialOverlapAccess(const MemoryAccess& ma, bool isPartialOverlap) :
            ma(ma),
            isPartialOverlap(isPartialOverlap){}

    public:
        PartialOverlapAccess(MemoryAccess& ma) : ma(ma), isPartialOverlap(false){}

        PartialOverlapAccess(const MemoryAccess&ma) : ma(ma), isPartialOverlap(false){}

        void flagAsPartial();

        const MemoryAccess& getAccess() const;

        bool getIsPartialOverlap() const;

        static set<PartialOverlapAccess> convertToPartialOverlaps(set<MemoryAccess>& s, bool arePartialOverlaps);

        static set<PartialOverlapAccess> convertToPartialOverlaps(set<MemoryAccess>& s);

        static void addToSet(set<PartialOverlapAccess>& ps, set<MemoryAccess>& s, bool arePartialOverlaps);

        static void addToSet(set<PartialOverlapAccess>& ps, set<MemoryAccess>& s);

        bool operator<(const PartialOverlapAccess& other) const;

        // Contained MemoryAccess structure delegation methods

        ADDRINT getIP() const;

        ADDRINT getActualIP() const;

        ADDRINT getAddress() const;

        long long int getSPOffset() const;

        long long int getBPOffset() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool getIsUninitializedRead() const;

        std::pair<int, int> getUninitializedInterval() const;
};