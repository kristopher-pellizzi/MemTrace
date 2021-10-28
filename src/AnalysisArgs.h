#include <set>
#include <sstream>
#include "pin.H"
#include "ShadowRegisterFile.h"

#ifndef ANALYSISARGS
#define ANALYSISARGS

using std::set;

class AnalysisArgs{
    private:
        set<REG>* regs;
        ADDRINT addr;
        UINT32 size;

    public:
        AnalysisArgs(set<REG>* regs, ADDRINT addr, UINT32 size);

        set<REG>* getRegs() const;
        ADDRINT getAddr() const ;
        UINT32 getSize() const;
        bool operator<(const AnalysisArgs& other) const;

        std::string toString();
};

#endif //ANALYSISARGS