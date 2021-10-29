#include <list>
#include "pin.H"
#include "../ShadowRegisterFile.h"

using std::pair;
using std::list;

#ifndef REGSSTATUS
#define REGSSTATUS

class RegsStatus{
    private:
        uint8_t* status;
        unsigned byteSize;
        unsigned shadowSize;
        bool allInitialized;

    public:
        RegsStatus(uint8_t* status, unsigned byteSize, unsigned shadowSize, bool allInitialized);
        ~RegsStatus();

        uint8_t* getStatus();
        unsigned getByteSize();
        unsigned getShadowSize();
        bool isAllInitialized();
};

RegsStatus getSrcRegsStatus(list<REG>* srcRegs);

#endif //REGSSTATUS