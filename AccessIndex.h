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
};