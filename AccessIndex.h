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
                int size = sizeof(size_t);
                size_t firstHash = std::hash<ADDRINT>()(ai.getFirst());

                return 
                    firstHash ^ ((firstHash << ai.getSecond()) | (unsigned)firstHash >> (size - ai.getSecond()));
            }
        };
};