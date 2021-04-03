#include "pin.H"
#include <iostream>

class AccessIndex{
    private:
        std::pair<ADDRINT, UINT32> elements;

    public:
        AccessIndex(ADDRINT first, UINT32 second){
            elements.first = first;
            elements.second = second;
        };
        
        AccessIndex(){};

        ADDRINT getFirst() const;

        UINT32 getSecond() const;

        bool operator<(const AccessIndex& other) const;

        bool operator==(const AccessIndex &other) const;

        bool operator!=(const AccessIndex &other) const;

        struct AIHasher{
            size_t operator()(const AccessIndex& ai) const{
                int size = sizeof(size_t) * 8;
                size_t firstHash = ai.getFirst();
                size_t shiftAmount = ai.getSecond() % (size > 4 ? (size - 4) : size);

                firstHash = firstHash ^ ((firstHash & ~((size_t) -1 << (size >> 1))) << (size - (size >> 1)));
                return 
                    ai.getFirst() + (((firstHash << shiftAmount) ^ (firstHash + ai.getSecond() - 1) << (size - shiftAmount)) | (firstHash >> (size - shiftAmount)));
            }
        };
};