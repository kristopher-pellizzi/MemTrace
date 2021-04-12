#include "pin.H"
#include <set>

#ifndef MEMORYACCESS
#define MEMORYACCESS

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
        MemoryAccess(){}

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

        bool operator== (const MemoryAccess& other) const;

        bool operator!= (const MemoryAccess& other) const;

        unsigned long long getOrder() const{
            return executionOrder;
        }

        friend struct ExecutionComparator;
        friend class MAHasher;



        // Define a functor class which allows to order MemoryAccess objects according to their execution order
        struct ExecutionComparator{
            bool operator()(const MemoryAccess& ma1, const MemoryAccess& ma2);
        };



        class MAHasher{
            private:
                int size;

                size_t lrot(size_t val, unsigned amount) const{
                    return (val << amount) | (val >> (size - amount));
                }

                size_t rrot(size_t val, unsigned amount) const{
                    return (val >> amount) | (val << (size - amount));
                }

            public:
                MAHasher(){
                    size = sizeof(size_t) * 8;
                }

                size_t operator()(const MemoryAccess& ma) const{
                    size_t addresses = ma.getIP() ^ lrot(ma.getActualIP(), (size >> 1)) ^ ma.executionOrder;
                    AccessType type = ma.getType();
                    unsigned long accessSize = ma.getSize();
                    size_t shiftAmount = accessSize % (size - 4);

                    addresses = lrot(addresses, shiftAmount);
                    addresses ^= lrot(ma.getAddress(), (size >> 1));
                    size_t mask = -1 >> (size - 4);
                    size_t partial = accessSize & mask;
                    size_t hash = partial;

                    for(int i = 4; i < size; i += 4){
                        hash = (hash << 4) | partial;
                    }

                    hash ^= addresses ^ ma.executionOrder;

                    hash = type == AccessType::READ ? lrot(hash, 8) : rrot(hash, 8);

                    if(ma.getIsUninitializedRead()){
                        hash = lrot(hash, (size >> 1));
                        std::pair<int, int> interval = ma.getUninitializedInterval();
                        mask = -1 >> (size - 10);
                        size_t intervalHash = 0;
                        for(int i=0; i < size; i += 20){
                            size_t partial = (interval.first & mask) | ((interval.second & mask) << 10);
                            partial <<= i;
                            intervalHash |= partial;
                        }
                        hash ^= intervalHash;
                    }

                    return rrot(hash, 16);
                }
        };




        class NoOrderHasher{
            private:
                int size;

                size_t lrot(size_t val, unsigned amount) const{
                    return (val << amount) | (val >> (size - amount));
                }

                size_t rrot(size_t val, unsigned amount) const{
                    return (val >> amount) | (val << (size - amount));
                }

            public:
                NoOrderHasher(){
                    size = sizeof(size_t) * 8;
                }

                size_t operator()(const MemoryAccess& ma) const{
                    size_t addresses = ma.getIP() ^ lrot(ma.getActualIP(), (size >> 1));
                    AccessType type = ma.getType();
                    unsigned long accessSize = ma.getSize();
                    size_t shiftAmount = accessSize % (size - 4);

                    addresses = lrot(addresses, shiftAmount);
                    addresses ^= lrot(ma.getAddress(), (size >> 1));
                    size_t mask = -1 >> (size - 4);
                    size_t partial = accessSize & mask;
                    size_t hash = partial;

                    for(int i = 4; i < size; i += 4){
                        hash = (hash << 4) | partial;
                    }

                    hash ^= addresses;

                    hash = type == AccessType::READ ? lrot(hash, 8) : rrot(hash, 8);

                    if(ma.getIsUninitializedRead()){
                        hash = lrot(hash, (size >> 1));
                        std::pair<int, int> interval = ma.getUninitializedInterval();
                        mask = -1 >> (size - 10);
                        size_t intervalHash = 0;
                        for(int i=0; i < size; i += 20){
                            size_t partial = (interval.first & mask) | ((interval.second & mask) << 10);
                            partial <<= i;
                            intervalHash |= partial;
                        }
                        hash ^= intervalHash;
                    }

                    return rrot(hash, 16);
                }
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

#endif // MEMORYACCESS