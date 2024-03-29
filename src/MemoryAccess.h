#ifndef MEMORYACCESS
#define MEMORYACCESS

#include "pin.H"
#include <set>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>

#include "ShadowMemory.h"

using std::set;

enum class AccessType{
    READ,
    WRITE
};

class MemoryAccess{
    protected:
        unsigned long long executionOrder;

    private:
        OPCODE opcode;
        ADDRINT instructionPointer;
        ADDRINT actualInstructionPointer;
        ADDRINT accessAddress;
        long long int spOffset;
        long long int bpOffset;
        UINT32 accessSize;
        AccessType type;
        std::string* instructionDisasm;
        bool isUninitializedRead;
        uint8_t* uninitializedInterval;
        ShadowBase* shadowMemory;

    public:
        // NOTE: this default constructor is never really useful. However, since we are using operator[] of a map
        // whose elements are MemoryAccess objects, we are required to provide a default constructor, and to initialize
        // its members (otherwise a warning will raise, and Intel PIN's default makefile considers all warnings as errors).
        MemoryAccess() :
            executionOrder(0),
            opcode(0),
            instructionPointer(0),
            actualInstructionPointer(0),
            accessAddress(0),
            spOffset(0),
            bpOffset(0),
            accessSize(0),
            type(AccessType::READ),
            instructionDisasm(NULL),
            isUninitializedRead(false),
            uninitializedInterval(NULL),
            shadowMemory(NULL)
            {}

        MemoryAccess(OPCODE opcode, unsigned long long executionOrder, ADDRINT ip, ADDRINT actualInstructionPointer, ADDRINT addr, int spOffset, int bpOffset, UINT32 size, AccessType type, std::string* disasm, ShadowBase* shadowMemory) : 
            executionOrder(executionOrder),
            opcode(opcode),
            instructionPointer(ip),
            actualInstructionPointer(actualInstructionPointer),
            accessAddress(addr),
            spOffset(spOffset),
            bpOffset(bpOffset),
            accessSize(size),
            type(type),
            instructionDisasm(disasm),
            isUninitializedRead(false),
            uninitializedInterval(NULL),
            shadowMemory(shadowMemory)
            {};

        MemoryAccess(const MemoryAccess& other);

        MemoryAccess(const MemoryAccess& other, UINT32 size);

        MemoryAccess(const MemoryAccess& other, UINT32 size, ADDRINT accessAddress);

        OPCODE getOpcode();
        
        ADDRINT getIP() const;

        ADDRINT getActualIP() const;

        ADDRINT getAddress() const;

        long long int getSPOffset() const;

        long long int getBPOffset() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool getIsUninitializedRead() const;

        uint8_t* getUninitializedInterval() const;

        ShadowBase* getShadowMemory() const;

        void setUninitializedInterval(uint8_t* interval);

        void setUninitializedRead();

        void setAsInitialized();

        bool isStackAccess() const;

        set<std::pair<unsigned, unsigned>> computeIntervals() const;

        std::string toString() const;

        void freeMemory() const;

        bool operator< (const MemoryAccess &other) const;

        bool operator== (const MemoryAccess& other) const;

        bool compare(const MemoryAccess& other) const;

        bool operator!= (const MemoryAccess& other) const;

        unsigned long long getOrder() const{
            return executionOrder;
        }

        friend struct ExecutionComparator;
        friend class MAHasher;



        struct Comparator{
            bool operator()(const MemoryAccess& ma1, const MemoryAccess& ma2) const{
                return ma1.compare(ma2);
            }
        };


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
                    size_t ret = ma.executionOrder ^ (ma.executionOrder >> size);
                    return ret;
                }
        };




        class NoOrderHasher{
            private:
                int size;

                size_t rrot(size_t val, unsigned amount) const{
                    return (val >> amount) | (val << (size - amount));
                }

            public:
                NoOrderHasher(){
                    size = sizeof(size_t) * 8;
                }

                size_t lrot(size_t val, unsigned amount) const{
                    return (val << amount) | (val >> (size - amount));
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

                        uint8_t* buf = (uint8_t*) malloc(size);
                        if((unsigned long long) size <= accessSize){
                            for(unsigned long long i = 0; i < accessSize; i += size){
                                unsigned long long copySize = i + size <= accessSize ? size : accessSize - i;

                                size_t v = * (size_t*) (ma.uninitializedInterval + i);
                                v &= (size_t) -1 >> (8 * (size - copySize));
                                hash ^= v;
                            }
                        }
                        else{
                            size_t v = *(size_t*) ma.uninitializedInterval;
                            v >>= (size - accessSize);
                            for(unsigned long long i = 0; i < (unsigned long long) size; i += accessSize){
                                hash ^= v;
                                v <<= (8 * accessSize);
                            }
                        }

                        free(buf);
                        buf = NULL;
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

        OPCODE getOpcode();
        
        ADDRINT getIP() const;

        ADDRINT getActualIP() const;

        ADDRINT getAddress() const;

        long long int getSPOffset() const;

        long long int getBPOffset() const;

        UINT32 getSize() const;

        AccessType getType() const;

        std::string getDisasm() const;

        bool getIsUninitializedRead() const;

        uint8_t* getUninitializedInterval() const;

        ShadowBase* getShadowMemory() const;

        bool isStackAccess() const;

        set<std::pair<unsigned, unsigned>> computeIntervals() const;
};

#endif // MEMORYACCESS