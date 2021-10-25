#include "AnalysisArgs.h"

AnalysisArgs::AnalysisArgs(set<REG>* regs, ADDRINT addr, UINT32 size) :
    regs(regs), 
    addr(addr), 
    size(size)
{}


set<REG>* AnalysisArgs::getRegs() const{
    return regs;
}


ADDRINT AnalysisArgs::getAddr() const{
    return addr;
}


UINT32 AnalysisArgs::getSize() const{
    return size;
}

bool AnalysisArgs::operator<(const AnalysisArgs& other) const{
    return this->addr > other.addr;
}