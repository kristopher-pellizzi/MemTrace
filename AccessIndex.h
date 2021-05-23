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

        class AIHasher{
            private:
                unsigned size;

                size_t lrot(size_t val, unsigned amount) const{
                    return (val << amount) | (val >> (size - amount));
                }

                size_t rrot(size_t val, unsigned amount) const{
                    return (val >> amount) | (val << (size - amount));
                }

                // Swaps bytes in pos1 and pos2, where 0 is the LSB.
                size_t swapBytes(size_t val, unsigned pos1, unsigned pos2) const{
                    if(pos1 == pos2)
                        return val;
                    
                    if(pos1 > pos2){
                        unsigned tmp = pos2;
                        pos2 = pos1;
                        pos1 = tmp;
                    }

                    pos1 *= 8;
                    pos2 *= 8;

                    size_t pos1Byte = (size_t) 0xff << pos1;
                    size_t pos2Byte = (size_t) 0xff << pos2;
                    size_t valCpy = val & ~pos1Byte & ~pos2Byte;
                    pos1Byte &= val;
                    pos2Byte &= val;
                    pos1Byte <<= pos2 - pos1;
                    pos2Byte >>= pos2 - pos1;
                    return valCpy ^ pos1Byte ^ pos2Byte;
                }
            
            public:
                AIHasher(){
                    size = sizeof(size_t) * 8;
                }

                size_t operator()(const AccessIndex& ai) const{
                    size_t hash = rrot(ai.getFirst(), 8);
                    size_t accessBits = 0;
                    UINT32 accessSize = ai.getSecond() << 24;
                    while(accessBits < size){
                        hash += (accessSize << accessBits);
                        accessBits += sizeof(accessSize) * 8;
                    }
                    hash = lrot(hash, 8);
                    hash ^= ai.getSecond();
                    hash = swapBytes(hash, 2, 3);
                    return hash;  
                }
        };
};