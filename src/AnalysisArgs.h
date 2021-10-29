#include <list>
#include <sstream>
#include "pin.H"
#include "ShadowRegisterFile.h"

#ifndef ANALYSISARGS
#define ANALYSISARGS

using std::list;

class AnalysisArgs{
    private:
        list<REG>* regs;
        ADDRINT addr;
        UINT32 size;

    public:
        AnalysisArgs(list<REG>* regs, ADDRINT addr, UINT32 size);

        list<REG>* getRegs() const;
        ADDRINT getAddr() const ;
        UINT32 getSize() const;
        bool operator<(const AnalysisArgs& other) const;

        std::string toString();
};

#endif //ANALYSISARGS