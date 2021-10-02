#include "PendingReads.h"

map<unsigned, set<std::pair<AccessIndex, MemoryAccess>>> pendingUninitializedReads;

/*
    Whenever an uninitialized read writes a register, all of its sub-registers are overwritten.
    So, add the entry to the destination registers and to all their sub-registers.
    E.g. if an instruction writes eax, it will also completely overwrite ax, al and ah, thus overwriting the 
    information about uninitialized bytes in there. rax, instead, remains untouched, as it may still have an 
    uninitialized byte in a position not included in eax.
    It is responsibility of funtion |checkDestRegisters| to perform the required operations to check if
    super-registers should be kept in the structure or should be removed.
*/
void addPendingRead(set<REG>* regs, const AccessIndex& ai, const MemoryAccess& ma){
    if(regs == NULL)
        return;
        
    std::pair<AccessIndex, MemoryAccess> entry(ai, ma);
    set<unsigned> toAdd;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    for(auto iter = regs->begin(); iter != regs->end(); ++iter){
        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        toAdd.insert(shadowReg);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        unsigned shadowByteSize = registerFile.getByteSize(*iter);

        for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
            SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
            // Add the same entry for each sub-register
            if(registerFile.getByteSize(shdw_reg) < shadowByteSize){
                toAdd.insert(*aliasReg);
            }
        }

    }

    for(auto iter = toAdd.begin(); iter != toAdd.end(); ++iter){
        auto pendingReadIter = pendingUninitializedReads.find(*iter);
        // If there's already an accessSet associated with the register, simply add the entry
        // to that set
        if(pendingReadIter != pendingUninitializedReads.end()){
            pendingUninitializedReads[*iter].insert(entry);
        }
        else{
            set<std::pair<AccessIndex, MemoryAccess>> s;
            s.insert(entry);
            pendingUninitializedReads[*iter] = s;
        }
    }
}

// Being an anonymous namespace, the following class is available only in this source file
namespace{
    class PendingReadsSorter{
        public:
            #define Access pair<AccessIndex, MemoryAccess>
            bool operator()(const Access& x, const Access& y){
                return x.second < y.second;
            }
            #undef Access
    };
}

static void addPendingRead(set<REG>* regs, set<pair<AccessIndex, MemoryAccess>, PendingReadsSorter>& accessSet){
    for(auto iter = accessSet.begin(); iter != accessSet.end(); ++iter){
        const AccessIndex& ai = iter->first;
        const MemoryAccess& ma = iter->second;
        addPendingRead(regs, ai, ma);
    }
}


void propagatePendingReads(set<REG>* srcRegs, set<REG>* dstRegs){
    // If srcRegs or dstRegs are empty, there's nothing to propagate
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    set<pair<AccessIndex, MemoryAccess>, PendingReadsSorter> accessSet;
    set<unsigned> toPropagate;

    // Select all registers which should be propagated
    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); iter++){
        // If source register is not uninitialized, it can't have accesses in |pendingUninitializedReads|
        if(!registerFile.isUninitialized(*iter))
            continue;

        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        unsigned byteSize = registerFile.getByteSize(*iter);
        toPropagate.insert(shadowReg);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);

        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            SHDW_REG aliasShdwReg = (SHDW_REG) *aliasIter;
            // If the alias register is a sub-register
            if(registerFile.getByteSize(aliasShdwReg) < byteSize){
                toPropagate.insert(*aliasIter);
            }
        }
    }

    for(auto iter = toPropagate.begin(); iter != toPropagate.end(); ++iter){
        auto pendingReadIter = pendingUninitializedReads.find(*iter);
        if(pendingReadIter != pendingUninitializedReads.end()){
            set<pair<AccessIndex, MemoryAccess>>& pendingReadSet = pendingReadIter->second;
            accessSet.insert(pendingReadSet.begin(), pendingReadSet.end());
        }
    }

    addPendingRead(dstRegs, accessSet);
}
