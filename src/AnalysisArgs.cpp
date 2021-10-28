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

std::string AnalysisArgs::toString(){
    std::ostringstream ss;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    ss << "Registers ";
    for(auto i = regs->begin(); i != regs->end(); ++i){
        ss << registerFile.getName(*i) << " ";
    }
    ss << "stored @ 0x" << std::hex << addr << std::endl;

    return ss.str();
}