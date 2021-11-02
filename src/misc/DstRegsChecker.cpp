#include "DstRegsChecker.h"

using std::endl;

#ifdef DEBUG
static std::ostream* out = &std::cerr;
#endif

map<OPCODE, unsigned> checkDestSize;


VOID checkDestRegisters(list<REG>* dstRegs, OPCODE opcode){
    if(dstRegs == NULL || pendingUninitializedReads.size() == 0)
        return;

    set<unsigned> toRemove;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    bool checkSuperRegisterCoverage = false;
    list<REG> singleByteRegs;

    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        if(registerFile.getByteSize(*iter) == 1){
            checkSuperRegisterCoverage = true;
            singleByteRegs.push_back(*iter);
        }
        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        toRemove.insert(shadowReg);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        // It's not a problem to tamper with destination register's content here,
        // because this function is called before |memtrace|, so the real status will be lately update.
        // Of course, this is valid only for destination registers. Other registers should not be modified.
        registerFile.setAsInitialized(*iter, opcode);

        for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
            SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
            // We must remove only those accesses that are not uninitialized anymore
            if(registerFile.isUninitialized(shdw_reg)){
                continue;
            }

            toRemove.insert(*aliasReg);
        }
    }

    TagManager& tagManager = TagManager::getInstance();

    for(auto iter = toRemove.begin(); iter != toRemove.end(); ++iter){
        /*
        If the destination register is a key in the pending reads map,
        remove its entry, as it is going to be overwritten, and it has never
        been used as a source register (possible false positive)
        */
        auto readIter = pendingUninitializedReads.find(*iter);
        if(readIter != pendingUninitializedReads.end()){
            set<tag_t>& tags = readIter->second;
            #ifdef DEBUG
            *out << "Removing pending reads for " << registerFile.getName((SHDW_REG)*iter) << endl;
            #endif
            tagManager.decreaseRefCount(tags);
            pendingUninitializedReads.erase(readIter);
        }
    }

    /* 
        Note that this should only happen rarely. Namely, it should happen when at least one of the destination registers
        is a 1 byte register (e.g. al, bl, bh...).
        This is required because some registers can write both byte 0 and 1 (e.g. rax can be referenced both as al and ah), 
        thus allowing to cover uninitialized bytes of previous loads with 2 different instructions. Example:
            Write rbx with uninitialized mask 11111100 (1 = initialized; 0 = uninitialized);
            Write bl with uninitialized mask 0;
            Write bh with uninitialized mask 0;
            The last 2 writes completely cover the first one, but since they happen at different instructions, the first
            write is not removed with the previous loop. We need an additional check.

    */
    if(checkSuperRegisterCoverage){
        set<unsigned> superRegs;
        set<unsigned> smallRegs;
        map<unsigned, uint8_t*> superRegsContent;

        for(auto iter = singleByteRegs.begin(); iter != singleByteRegs.end(); ++iter){
            unsigned shadowByteSize = 1;
            smallRegs.insert(registerFile.getShadowRegister(*iter));
            set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);

            // Look for another 1 byte register in |pendingUninitializedReads|
            for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
                SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
                auto pendingReadsIter = pendingUninitializedReads.find(*aliasReg);
                if(pendingReadsIter == pendingUninitializedReads.end())
                    continue;

                if(registerFile.getByteSize(shdw_reg) == shadowByteSize)
                    smallRegs.insert(*aliasReg);
                else{
                    uint8_t* content = registerFile.getContentStatus(shdw_reg);
                    bool toCheck = true;
                    unsigned shadowSize = registerFile.getShadowSize(shdw_reg);
                    for(unsigned i = 0; i < shadowSize - 1 && toCheck; ++i){
                        if(*(content + i) != 0xff)
                            toCheck = false;
                    }

                    if(toCheck){
                        superRegs.insert(*aliasReg);
                        superRegsContent[*aliasReg] = content;
                    }
                    else{
                        free(content);
                    }
                }
            }
        }

        if(superRegs.size() == 0){
            return;
        }

        toRemove.clear();
        uint8_t smallRegsMask = 0;
        for(auto iter = smallRegs.begin(); iter != smallRegs.end(); ++iter){
            SHDW_REG shdw_reg = (SHDW_REG) *iter;
            uint8_t* content = registerFile.getContentStatus(shdw_reg);
            smallRegsMask |= ~(*content);
            free(content);
        }

        for(auto iter = superRegs.begin(); iter != superRegs.end(); ++iter){
            SHDW_REG shdw_reg = (SHDW_REG) *iter;
            uint8_t* content = superRegsContent[*iter];
            unsigned shadowSize = registerFile.getShadowSize(shdw_reg);
            uint8_t* firstBytePtr = content + shadowSize - 1;
            
            uint8_t regMask = *firstBytePtr;
            regMask |= smallRegsMask;

            // Small registers completely cover this register
            if(regMask == 0xff){
                toRemove.insert(*iter);
            }
            free(content);
        }

        for(auto iter = toRemove.begin(); iter != toRemove.end(); ++iter){
            auto readIter = pendingUninitializedReads.find(*iter);
            if(readIter != pendingUninitializedReads.end()){
                set<tag_t>& tags = readIter->second;
                #ifdef DEBUG
                *out << "Removing pending reads for " << registerFile.getName((SHDW_REG)*iter) << endl;
                #endif
                tagManager.decreaseRefCount(tags);
                pendingUninitializedReads.erase(readIter);
            }
        }
    }
}


VOID checkDestRegisters(list<REG>* dstRegs, OPCODE opcode, unsigned bits){
    if(dstRegs == NULL || pendingUninitializedReads.size() == 0)
        return;

    set<unsigned> toRemove;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    bool checkSuperRegisterCoverage = false;
    list<REG> singleByteRegs;
    unsigned bytes = bits % 8 != 0 ? (bits / 8) + 1 : bits / 8;
    unsigned shadowBytes = bytes / 8;
    bool additionalByte = bytes % 8 != 0;

    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        if(registerFile.getByteSize(*iter) == 1){
            checkSuperRegisterCoverage = true;
            singleByteRegs.push_back(*iter);
        }
        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        uint8_t* regStatus = registerFile.getContentStatus(*iter);
        unsigned dstShadowSize = registerFile.getShadowSize(*iter);
        uint8_t* dstStatus = (uint8_t*) malloc(sizeof(uint8_t) * dstShadowSize);
        uint8_t* dstPtr = dstStatus + dstShadowSize - 1;
        uint8_t* srcPtr = regStatus + dstShadowSize - 1;
        for(unsigned i = 0; i < shadowBytes; ++i, --dstPtr, --srcPtr){
            *dstPtr = 0xff;
        }

        if(additionalByte){
            uint8_t mask = 0xff;
            uint8_t srcByte = *srcPtr | (mask >> (8 - (bytes % 8)));
            *dstPtr = srcByte;
            --dstPtr;
            --srcPtr;
        }

        for(unsigned i = shadowBytes; i < dstShadowSize; ++i, --dstPtr, --srcPtr){
            *dstPtr = *srcPtr;
        }

        // It's not a problem to tamper with destination register's content here,
        // because this function is called before |memtrace|, so the real status will be lately update.
        // Of course, this is valid only for destination registers. Other registers should not be modified.
        registerFile.setAsInitialized(*iter, opcode, dstStatus);

        free(dstStatus);
        free(regStatus);

        if(!registerFile.isUninitialized((SHDW_REG) shadowReg))
            toRemove.insert(shadowReg);

        for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
            SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
            // We must remove only those accesses that are not uninitialized anymore
            if(registerFile.isUninitialized(shdw_reg)){
                continue;
            }

            toRemove.insert(*aliasReg);
        }
    }

    TagManager& tagManager = TagManager::getInstance();

    for(auto iter = toRemove.begin(); iter != toRemove.end(); ++iter){
        /*
        If the destination register is a key in the pending reads map,
        remove its entry, as it is going to be overwritten, and it has never
        been used as a source register (possible false positive)
        */
        auto readIter = pendingUninitializedReads.find(*iter);
        if(readIter != pendingUninitializedReads.end()){
            set<tag_t>& tags = readIter->second;
            #ifdef DEBUG
            *out << "Removing pending reads for " << registerFile.getName((SHDW_REG)*iter) << endl;
            #endif
            tagManager.decreaseRefCount(tags);
            pendingUninitializedReads.erase(readIter);
        }
    }

    /* 
        Note that this should only happen rarely. Namely, it should happen when at least one of the destination registers
        is a 1 byte register (e.g. al, bl, bh...).
        This is required because some registers can write both byte 0 and 1 (e.g. rax can be referenced both as al and ah), 
        thus allowing to cover uninitialized bytes of previous loads with 2 different instructions. Example:
            Write rbx with uninitialized mask 11111100 (1 = initialized; 0 = uninitialized);
            Write bl with uninitialized mask 0;
            Write bh with uninitialized mask 0;
            The last 2 writes completely cover the first one, but since they happen at different instructions, the first
            write is not removed with the previous loop. We need an additional check.

    */
    if(checkSuperRegisterCoverage){
        set<unsigned> superRegs;
        set<unsigned> smallRegs;
        map<unsigned, uint8_t*> superRegsContent;

        for(auto iter = singleByteRegs.begin(); iter != singleByteRegs.end(); ++iter){
            unsigned shadowByteSize = 1;
            smallRegs.insert(registerFile.getShadowRegister(*iter));
            set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);

            // Look for another 1 byte register in |pendingUninitializedReads|
            for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
                SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
                auto pendingReadsIter = pendingUninitializedReads.find(*aliasReg);
                if(pendingReadsIter == pendingUninitializedReads.end())
                    continue;

                if(registerFile.getByteSize(shdw_reg) == shadowByteSize)
                    smallRegs.insert(*aliasReg);
                else{
                    uint8_t* content = registerFile.getContentStatus(shdw_reg);
                    bool toCheck = true;
                    unsigned shadowSize = registerFile.getShadowSize(shdw_reg);
                    for(unsigned i = 0; i < shadowSize - 1 && toCheck; ++i){
                        if(*(content + i) != 0xff)
                            toCheck = false;
                    }

                    if(toCheck){
                        superRegs.insert(*aliasReg);
                        superRegsContent[*aliasReg] = content;
                    }
                    else{
                        free(content);
                    }
                }
            }
        }

        if(superRegs.size() == 0){
            return;
        }

        toRemove.clear();
        uint8_t smallRegsMask = 0;
        for(auto iter = smallRegs.begin(); iter != smallRegs.end(); ++iter){
            SHDW_REG shdw_reg = (SHDW_REG) *iter;
            uint8_t* content = registerFile.getContentStatus(shdw_reg);
            smallRegsMask |= ~(*content);
            free(content);
        }

        for(auto iter = superRegs.begin(); iter != superRegs.end(); ++iter){
            SHDW_REG shdw_reg = (SHDW_REG) *iter;
            uint8_t* content = superRegsContent[*iter];
            unsigned shadowSize = registerFile.getShadowSize(shdw_reg);
            uint8_t* firstBytePtr = content + shadowSize - 1;
            
            uint8_t regMask = *firstBytePtr;
            regMask |= smallRegsMask;

            // Small registers completely cover this register
            if(regMask == 0xff){
                toRemove.insert(*iter);
            }
            free(content);
        }

        for(auto iter = toRemove.begin(); iter != toRemove.end(); ++iter){
            auto readIter = pendingUninitializedReads.find(*iter);
            if(readIter != pendingUninitializedReads.end()){
                set<tag_t>& tags = readIter->second;
                #ifdef DEBUG
                *out << "Removing pending reads for " << registerFile.getName((SHDW_REG)*iter) << endl;
                #endif
                tagManager.decreaseRefCount(tags);
                pendingUninitializedReads.erase(readIter);
            }
        }
    }
}