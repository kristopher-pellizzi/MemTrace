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

static void addPendingRead(set<unsigned>& shdw_regs, set<pair<AccessIndex, MemoryAccess>>& accessSet){
    set<unsigned> toAdd;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    for(auto iter = shdw_regs.begin(); iter != shdw_regs.end(); ++iter){
        toAdd.insert(*iter);
        SHDW_REG shdw_iter = (SHDW_REG) *iter;
        unsigned byteSize = registerFile.getByteSize(shdw_iter);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(shdw_iter);
        
        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            SHDW_REG shdw_alias = (SHDW_REG) *aliasIter;
            if(registerFile.getByteSize(shdw_alias) < byteSize){
                toAdd.insert(*aliasIter);
            }
        }
    }

    for(auto iter = toAdd.begin(); iter != toAdd.end(); ++iter){
        for(auto accessIter = accessSet.begin(); accessIter != accessSet.end(); ++accessIter){
            const pair<AccessIndex, MemoryAccess>& entry = *accessIter;

            /*
                In this case, we must replace the possible accesses stored for a register with a new set.
                Indeed, since this function is called by iterating on registers sorted decreasingly according to
                their size in bytes, if we are propagating access sets to any sub-register, we must replace its access set
                with the access set stored for the corresponding src sub-register, which may be differente from the access 
                set of the parent register.
            */
            set<std::pair<AccessIndex, MemoryAccess>> s;
            s.insert(entry);
            pendingUninitializedReads[*iter] = s;
        }
    }
}


void propagatePendingReads(set<REG>* srcRegs, set<REG>* dstRegs){
    // If srcRegs or dstRegs are empty, there's nothing to propagate
    if(srcRegs == NULL || dstRegs == NULL)
        return;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    set<unsigned, ShadowRegisterFile::DecresingSizeRegisterSorter> toPropagate;

    // Select all registers which should be propagated
    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); iter++){
        // If source register is not uninitialized, it can't have accesses in |pendingUninitializedReads|
        if(!registerFile.isUninitialized(*iter) || registerFile.isUnknownRegister(*iter))
            continue;

        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        unsigned byteSize = registerFile.getByteSize(*iter);
        set<pair<AccessIndex, MemoryAccess>>* pendingReadSetPtr = NULL;
        auto pendingReadIter = pendingUninitializedReads.find(shadowReg);
        if(pendingReadIter != pendingUninitializedReads.end()){
            set<pair<AccessIndex, MemoryAccess>>& pendingReadSet = pendingReadIter->second;
            pendingReadSetPtr = &pendingReadSet;
            set<unsigned> correspondingRegisters = registerFile.getCorrespondingRegisters((SHDW_REG) shadowReg, dstRegs);
            
            addPendingRead(correspondingRegisters, pendingReadSet);
        }

        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            SHDW_REG aliasShdwReg = (SHDW_REG) *aliasIter;

            // If the alias register is a sub-register
            if(registerFile.getByteSize(aliasShdwReg) < byteSize){
                pendingReadIter = pendingUninitializedReads.find(*aliasIter);
                // If it has an associated pending uninitialized read (it should have, this is simply to avoid run-time errors)
                if(pendingReadIter != pendingUninitializedReads.end()){
                    set<pair<AccessIndex, MemoryAccess>>& aliasPendingReadSet = pendingReadIter->second;
                    // Add to the set of registers still to be propagated only those sub-registers whose associated access set
                    // is different from the access set of the src register
                    if(&aliasPendingReadSet != pendingReadSetPtr){
                        toPropagate.insert(*aliasIter);
                    }
                }
            }
        }
    }

    // We must propagate changes done to sub-registers of src, if any
    for(auto iter = toPropagate.begin(); iter != toPropagate.end(); ++iter){
        auto pendingReadIter = pendingUninitializedReads.find(*iter);
        if(pendingReadIter != pendingUninitializedReads.end()){
            SHDW_REG shdw_iter = (SHDW_REG) *iter;
            set<pair<AccessIndex, MemoryAccess>>& pendingReadSet = pendingReadIter->second;
            set<unsigned> correspondingRegisters = registerFile.getCorrespondingRegisters(shdw_iter, dstRegs);

            addPendingRead(correspondingRegisters, pendingReadSet);
        }
    }
}
