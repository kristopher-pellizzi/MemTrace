#include <iostream>
#include <cstring>
#include "ShadowRegisterFile.h"
#include "misc/InstructionClassification.h"


/*
    The following set contains those shadow registers whose size is 1 byte, but that refer to the 
    High byte (i.e. if bytes are numbered from 0 starting from the least significant bytes, these
    registers refer to byte 1 of their super-register).

    Example:
    SHDW_REG_RAX is 64 bits long, so it has bytes numbered as [7:0].
    SHDW_REG_AL refers to byte 0, while SHDW_REG_AH refers to byte 1 of SHDW_REG_RAX.
*/
static set<SHDW_REG> requireAdjustment;

ShadowRegisterFile::ShadowRegisterFile(){
    init();
}

ShadowRegisterFile::~ShadowRegisterFile(){
    // De-allocate memory page reserved for shadow register file
    free(shadowRegistersPtr);

    // De-allocate all shadow registers dynamically allocated during initialization
    for(unsigned i = 0; i < numRegisters; ++i){
        delete(shadowRegisters[i]);
    }

    // De-allocate shadow registers array
    free(shadowRegisters);
}

void ShadowRegisterFile::init(){
    #define X(v, ...) + 1
    numRegisters = 0 REG_TABLE(X);
    #undef X

    initMap();
    initRequireAdjustment();
    initShadowRegisters();
}

void ShadowRegisterFile::initRequireAdjustment(){
    requireAdjustment.insert(SHDW_REG_AH);
    requireAdjustment.insert(SHDW_REG_BH);
    requireAdjustment.insert(SHDW_REG_CH);
    requireAdjustment.insert(SHDW_REG_DH);
}


// ShadowRegisters size is expressed in bits (e.g. size=64 means the register is a 64 bits register)
// Note: we are assuming all registers are at least 64 bits long. If that's not the case, we might need to
// adjust the increase of the regContents pointer
ShadowRegister* ShadowRegisterFile::getNewShadowRegister(const char* name, unsigned size, uint8_t** regContents){
    ShadowRegister* ret = new ShadowRegister(name, size, *regContents);

    // Note that all registers have a size divisible by 8, so the integer result obtained is an exact result
    unsigned byteSize = ceilToMultipleOf8(size / 8);
    (*regContents) += (byteSize / 8);

    return ret;
}


ShadowRegister* ShadowRegisterFile::getNewShadowSubRegister(const char* name, unsigned size, SHDW_REG idx, SHDW_REG superRegister, bool isOverwritingReg){
    ShadowRegister* parentRegister = shadowRegisters[superRegister];
    aliasingRegisters[superRegister].insert((unsigned)idx);

    unsigned byteSize = size / 8 / 8;
    // This happens when the subregister has a size lower than 8 bytes. In this case, we must take the least 
    // significant byte of the super-register content
    if(byteSize == 0)
        byteSize = 1;
    uint8_t* contentPtr = parentRegister->getContentPtr() + (ceilToMultipleOf8(parentRegister->getSize() / 8) / 8) - byteSize;
    

    ShadowRegister* ret;
    if(requireAdjustment.find(idx) != requireAdjustment.end()){
        ret = new ShadowHighByteSubRegister(name, size, contentPtr);
        haveHighByte.insert(idx);
    }
    else if(isOverwritingReg){
        if(idx >= SHDW_REG_XMM0 && idx <= SHDW_REG_XMM31){
            ret = new ShadowHybridRegister(name, size, contentPtr, parentRegister->getContentPtr());
        }
        else{
            ret = new ShadowOverwritingSubRegister(name, size, contentPtr, parentRegister->getContentPtr());
        }
    }
    else{
        ret = new ShadowRegister(name, size, contentPtr);
    }

    return ret;
}

void ShadowRegisterFile::initShadowRegistersPtr(unsigned* sizes, SHDW_REG* superRegisters){
    set<SHDW_REG> regs;
    size_t allocationSize = 0;

    for(unsigned i = 0; i < numRegisters; ++i){
        std::pair<set<SHDW_REG>::iterator, bool> res = regs.insert(superRegisters[i]);
        if(res.second){
            unsigned curr_size = ceilToMultipleOf8(sizes[i] / 8);
            // Add to the allocation size the size in bytes of the register divided by 8 (to reserve a single bit to 
            // every byte of the register)
            allocationSize += (curr_size / 8);
        }
    }
    shadowRegistersPtr = malloc(sizeof(uint8_t) * allocationSize);
    memset(shadowRegistersPtr, 0xff, allocationSize);
}


void ShadowRegisterFile::initShadowRegisters(){
    
    shadowRegisters = (ShadowRegister**) malloc(sizeof(ShadowRegister*) * numRegisters);

    #define X(v, ...) #v, 
    const char* names[] = {REG_TABLE(X)};
    #undef X

    #define X(name, size, ...) size, 
    unsigned sizes[] = {REG_TABLE(X)};
    #undef X

    #define X(name, size, superReg, isOverwritingReg) isOverwritingReg,
    bool overwritingRegisters[] = {REG_TABLE(X)};
    #undef X

    #define X(name, size, superRegister, ...) superRegister, 
    SHDW_REG superRegisters[] = {REG_TABLE(X)};
    #undef X

    initShadowRegistersPtr(sizes, superRegisters);
    void* nextRegisterContentPtr = shadowRegistersPtr;


    for(unsigned i = 0; i < numRegisters; ++i){
        ShadowRegister* shadowReg;

        // If register is the biggest of a group of aliasing registers
        // (e.g. SHDW_REG_RAX is the biggest among SHDW_REG_RAX, SHDW_REG_EAX, SHDW_REG_AX, SHDW_REG_AL, SHDW_REG_AH).
        if(i == (unsigned) superRegisters[i]){
            shadowReg = getNewShadowRegister(names[i], sizes[i], (uint8_t**)&nextRegisterContentPtr);
        }
        else{
            shadowReg = getNewShadowSubRegister(names[i], sizes[i], (SHDW_REG) i, superRegisters[i], overwritingRegisters[i]);
        }

        shadowRegisters[i] = shadowReg;
    }

    // Populate |aliasingRegisters| map
    map<SHDW_REG, set<unsigned>> toInsert;
    for(auto superRegsIter = aliasingRegisters.begin(); superRegsIter != aliasingRegisters.end(); ++superRegsIter){
        set<unsigned>& aliasSet = superRegsIter->second;
        for(auto aliasReg = aliasSet.begin(); aliasReg != aliasSet.end(); ++aliasReg){
            toInsert[(SHDW_REG) *aliasReg].insert((unsigned) superRegsIter->first);

            for(auto iter = aliasSet.begin(); iter != aliasSet.end(); ++iter){
                // Don't insert a register as an alias of itself
                if(*iter == *aliasReg)
                    continue;

                toInsert[(SHDW_REG) *aliasReg].insert(*iter);
            }
        }
    }

    aliasingRegisters.insert(toInsert.begin(), toInsert.end());


    // Populate set |haveHighByte|
    set<SHDW_REG> tmpSet;
    for(auto iter = haveHighByte.begin(); iter != haveHighByte.end(); ++iter){
        set<unsigned>& aliasingRegs = aliasingRegisters[*iter];
        for(auto aliasIter = aliasingRegs.begin(); aliasIter != aliasingRegs.end(); ++aliasIter){
            SHDW_REG shdw_reg = (SHDW_REG) *aliasIter;
            tmpSet.insert(shdw_reg);
        }
    }

    haveHighByte.insert(tmpSet.begin(), tmpSet.end());
    
}


/*
    Converts a PIN register into a Shadow register.
    To emulate the behavior of the FPU stack, the fpu stack index is always added to registers (st0 ~ st7), so that
    the shadow register file reflects the actual state of the real fpu stack.
*/
SHDW_REG ShadowRegisterFile::convertPinReg(REG pin_reg){
    auto ret = shadow_map.find(pin_reg);

    if(ret != shadow_map.end()){
        SHDW_REG reg = ret->second;
        
        // Whenever an MM register is read or written, the fpuStackIndex is reset
        if(reg >= SHDW_REG_MM0 && reg <= SHDW_REG_MM7){
            fpuStackIndex = 0;
        }
        else if(reg >= SHDW_REG_ST0 && reg <= SHDW_REG_ST7){
            reg += fpuStackIndex;
            if(reg > SHDW_REG_ST7){
                reg -= SHDW_REG_ST7;
                reg += -1; // Required because reg is already higher than SHDW_REG_ST7.
                reg += SHDW_REG_ST0;
            }
            else if(reg < SHDW_REG_ST0){
                SHDW_REG diff = SHDW_REG_ST0 - reg;
                diff -= 1; // Required because diff range starts from 1. In that case, reg is exactly SHDW_REG_ST7
                reg = SHDW_REG_ST7 - diff;
            }
        }

        return reg;
    }

    #ifdef DEBUG
    if(alreadyNotified.find(pin_reg) == alreadyNotified.end()){
        std::cerr << "No SHDW_REG corresponding to register " << REG_StringShort(pin_reg) << std::endl;
        alreadyNotified.insert(pin_reg);
    }
    #endif

    return (SHDW_REG) -1;
}


string& ShadowRegisterFile::getName(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1){
        return unknownString;
    }

    return shadowRegisters[shadow_reg]->getName();
}


string& ShadowRegisterFile::getName(SHDW_REG reg){
    if(reg < numRegisters)
        return shadowRegisters[reg]->getName();
    else    
        return unknownString;
}


void ShadowRegisterFile::setAsInitialized(REG pin_reg, uint8_t* data){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);
    
    if(shadow_reg == (SHDW_REG) -1){
        return;
    }
    shadowRegisters[shadow_reg]->setAsInitialized(data);
}


void ShadowRegisterFile::setAsInitialized(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1){
        return;
    }

    shadowRegisters[shadow_reg]->setAsInitialized();
}




void ShadowRegisterFile::setAsInitialized(REG pin_reg, OPCODE opcode){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return;

    // This can't be an hybrid register => just use the method's version which does not use 
    // the OPCODE parameter
    if(shadow_reg < SHDW_REG_XMM0 || shadow_reg > SHDW_REG_XMM31){
        return setAsInitialized(pin_reg);
    }

    ShadowHybridRegister* reg = (ShadowHybridRegister*) shadowRegisters[shadow_reg];
    ShadowHybridRegister::BehaviorSelector selector;

    if(isSSEInstruction(opcode))
        selector = ShadowHybridRegister::BehaviorSelector::NORMAL;
    else
        selector = ShadowHybridRegister::BehaviorSelector::OVERWRITING;

    reg->setAsInitialized(selector);
}

void ShadowRegisterFile::setAsInitialized(REG pin_reg, OPCODE opcode, uint8_t* data){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return;

    if(shadow_reg < SHDW_REG_XMM0 || shadow_reg > SHDW_REG_XMM31){
        return setAsInitialized(pin_reg, data);
    }

    ShadowHybridRegister* reg = (ShadowHybridRegister*) shadowRegisters[shadow_reg];
    ShadowHybridRegister::BehaviorSelector selector;

    if(isSSEInstruction(opcode))
        selector = ShadowHybridRegister::BehaviorSelector::NORMAL;
    else
        selector = ShadowHybridRegister::BehaviorSelector::OVERWRITING;

    reg->setAsInitialized(data, selector);
}


void ShadowRegisterFile::setBitsAsInitialized(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) - 1)
        return;

    if(isHighByteReg(shadow_reg))
        shadowRegisters[shadow_reg]->setAsInitialized();
    else
        shadowRegisters[shadow_reg]->ShadowRegister::setAsInitialized();
}


void ShadowRegisterFile::setBitsAsInitialized(SHDW_REG reg){
    if(reg >= numRegisters)
        return;

    if(isHighByteReg(reg))
        shadowRegisters[reg]->setAsInitialized();
    else
        shadowRegisters[reg]->ShadowRegister::setAsInitialized();
}


void ShadowRegisterFile::setBitsAsInitialized(REG pin_reg, uint8_t* data){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return;

    if(isHighByteReg(shadow_reg))
        shadowRegisters[shadow_reg]->setAsInitialized(data);
    else
        shadowRegisters[shadow_reg]->ShadowRegister::setAsInitialized(data);
}


void ShadowRegisterFile::setBitsAsInitialized(list<REG>* regs){
    for(auto iter = regs->begin(); iter != regs->end(); ++iter){
        setBitsAsInitialized(*iter);
    }
}


uint8_t* ShadowRegisterFile::getContentStatus(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1){
        // Note: the caller should control the returned pointer to avoid segmentation faults
        return NULL;
    }

    return shadowRegisters[shadow_reg]->getContentStatus();
}


uint8_t* ShadowRegisterFile::getContentStatus(SHDW_REG reg){
    return shadowRegisters[reg]->getContentStatus();
}


unsigned ShadowRegisterFile::getShadowSize(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return -1;
    
    return shadowRegisters[shadow_reg]->getShadowSize();
}


unsigned ShadowRegisterFile::getShadowSize(SHDW_REG reg){
    return shadowRegisters[reg]->getShadowSize();
}


unsigned ShadowRegisterFile::getByteSize(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return -1;

    return shadowRegisters[shadow_reg]->getByteSize();
}

unsigned ShadowRegisterFile::getByteSize(SHDW_REG reg){
    return shadowRegisters[reg]->getByteSize();
}


unsigned ShadowRegisterFile::getShadowRegister(REG pin_reg){
    SHDW_REG ret = convertPinReg(pin_reg);

    if(ret != (SHDW_REG) -1)
        return ret;

    unsigned newId = numRegisters + unknownRegisters.size();
    unknownRegisters[pin_reg] = newId;
    return newId;
}


bool ShadowRegisterFile::isUninitialized(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return false;

    return shadowRegisters[shadow_reg]->isUninitialized();
}


bool ShadowRegisterFile::isUninitialized(SHDW_REG reg){
    return shadowRegisters[reg]->isUninitialized();
}


bool ShadowRegisterFile::isUnknownRegister(REG pin_reg){
    SHDW_REG shdw_reg = convertPinReg(pin_reg);

    return shdw_reg == (SHDW_REG) -1;
}


set<unsigned>& ShadowRegisterFile::getAliasingRegisters(REG pin_reg){
    SHDW_REG shadow_reg = convertPinReg(pin_reg);

    if(shadow_reg == (SHDW_REG) -1)
        return emptySet;

    return aliasingRegisters[shadow_reg];
}

set<unsigned>& ShadowRegisterFile::getAliasingRegisters(SHDW_REG reg){
    return aliasingRegisters[reg];
}

bool ShadowRegisterFile::DecresingSizeRegisterSorter::operator()(const unsigned x, const unsigned y){
    SHDW_REG shdw_x = (SHDW_REG) x;
    SHDW_REG shdw_y = (SHDW_REG) y;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    unsigned xByteSize = registerFile.getByteSize(shdw_x);
    unsigned yByteSize = registerFile.getByteSize(shdw_y);

    if(xByteSize != yByteSize){
        return xByteSize > yByteSize;
    }

    return registerFile.isHighByteReg(shdw_x);
}


bool ShadowRegisterFile::IncreasingSizeRegisterSorter::operator()(const unsigned x, const unsigned y){
    SHDW_REG shdw_x = (SHDW_REG) x;
    SHDW_REG shdw_y = (SHDW_REG) y;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    unsigned xByteSize = registerFile.getByteSize(shdw_x);
    unsigned yByteSize = registerFile.getByteSize(shdw_y);

    if(xByteSize != yByteSize){
        return xByteSize < yByteSize;
    }

    return !registerFile.isHighByteReg(shdw_x);
}

/*
    Given a SHDW_REG |reg|, and a set of registers, get a set of registers with these properties:
    [*] Each returned register is the alias register of 1 of the given registers
    [*] Each returned register corresponds to |reg| (same size or different size but missing destination equivalent. 
        E.g. |reg| is bh, |regSet| only contains rsi. In that case, the corresponding register will be si)
*/
set<unsigned> ShadowRegisterFile::getCorrespondingRegisters(SHDW_REG reg, list<REG>* regSet){

    set<unsigned> corrRegs;
    unsigned targetByteSize;
    bool targetIsHighByte = false;
    bool targetFound = false;
    unsigned lastCheckedRegister;

    targetByteSize = getByteSize(reg);

    if(targetByteSize == 1 && shadowRegisters[reg]->isHighByte()){
        targetIsHighByte = true; 
    }

    for(auto iter = regSet->begin(); iter != regSet->end(); ++iter){
        if(getByteSize(*iter) < targetByteSize){
            if(!isUnknownRegister(*iter))
                corrRegs.insert(getShadowRegister(*iter));
            continue;
        }

        // If |reg| is a high byte register (e.g. ah) and the dst register does not have an aliasing high byte register,
        // set as the corresponding register the one whose size is 2 bytes
        if(targetIsHighByte && !hasHighByte(*iter)){
            targetByteSize = 2;
        }


        set<unsigned>& tmpAliasing = getAliasingRegisters(*iter);
        set<unsigned, ShadowRegisterFile::DecresingSizeRegisterSorter> aliasRegs;
        aliasRegs.insert(tmpAliasing.begin(), tmpAliasing.end());

        if(!isUnknownRegister(*iter))
            aliasRegs.insert(getShadowRegister(*iter));

        for(auto aliasReg = aliasRegs.begin(); aliasReg != aliasRegs.end(); ++aliasReg){
            SHDW_REG shdw_alias = (SHDW_REG) *aliasReg;
            unsigned aliasByteSize = getByteSize(shdw_alias);

            if(aliasByteSize >= targetByteSize){
                lastCheckedRegister = *aliasReg;
            }

            if(aliasByteSize == targetByteSize){
                // If |reg| is a high byte register and the current alias register is a low byte register,
                // we must keep searching for the correct corresponding register
                if(targetIsHighByte){
                    if(!shadowRegisters[shdw_alias]->isHighByte()){
                        continue;
                    }
                }
                else{
                    if(shadowRegisters[shdw_alias]->isHighByte()){
                        continue;
                    }
                }

                corrRegs.insert(*aliasReg);
                targetFound = true;
                break;
            }
            
        }

        if(!targetFound){
            corrRegs.insert(lastCheckedRegister);
        }
    }

    return corrRegs;
}


bool ShadowRegisterFile::hasHighByte(REG pin_reg){
    SHDW_REG reg = convertPinReg(pin_reg);
    if(reg == (SHDW_REG) -1){
        return false;
    }

    return haveHighByte.find(reg) != haveHighByte.end();
}


bool ShadowRegisterFile::isHighByteReg(SHDW_REG reg){
    return shadowRegisters[reg]->isHighByte();
}


void ShadowRegisterFile::decrementFpuStackIndex(){
    if(--fpuStackIndex < 0){
        fpuStackIndex += 8;
    }
}

void ShadowRegisterFile::incrementFpuStackIndex(){
    if(++fpuStackIndex > 7){
        fpuStackIndex -= 8;
    }
}


SHDW_REG& operator+=(SHDW_REG& x, const SHDW_REG& y){
    unsigned intX = (unsigned) x;
    unsigned intY = (unsigned) y;
    unsigned ret = intX + intY;

    x = (SHDW_REG) ret;
    return x;
}


SHDW_REG& operator+=(SHDW_REG& x, const unsigned y){
    unsigned intX = (unsigned) x;
    unsigned ret = intX + y;

    x = (SHDW_REG) ret;
    return x;
}

SHDW_REG& operator-=(SHDW_REG& x, const unsigned y){
    unsigned intX = (unsigned) x;
    unsigned ret = intX - y;

    x = (SHDW_REG) ret;
    return x;
}

SHDW_REG& operator-=(SHDW_REG& x, const SHDW_REG& y){
    unsigned intX = (unsigned) x;
    unsigned intY = (unsigned) y;
    unsigned ret = intX - intY;

    x = (SHDW_REG) ret;
    return x;
}

SHDW_REG operator-(const SHDW_REG& x, SHDW_REG y){
    unsigned intX = (unsigned) x;
    unsigned intY = (unsigned) y;
    unsigned ret = intX - intY;

    return (SHDW_REG) ret;
}

SHDW_REG operator+(const SHDW_REG& x, SHDW_REG y){
    unsigned intX = (unsigned) x;
    unsigned intY = (unsigned) y;
    unsigned ret = intX + intY;

    return (SHDW_REG) ret;
}



/*
    In this case we can't use macro |RegTable| because some Intel PIN's registers are not always defined, as it
    depends from the underlined architecture.
    So, the map will contain only the registers actually defined by Intel PIN, while the shadow register file
    will contain every possible register.
    This is not a problem, as we allocate a whole memory page for the shadow register file only once, so that 
    it does not require to allocate space for each register singularly.
*/
void ShadowRegisterFile::initMap(){
#ifdef TARGET_IA32E
    shadow_map[REG_RIP] = SHDW_REG_RIP;

    shadow_map[REG_RAX] = SHDW_REG_RAX;
    
    shadow_map[REG_RBX] = SHDW_REG_RBX;

    shadow_map[REG_RCX] = SHDW_REG_RCX;

    shadow_map[REG_RDX] = SHDW_REG_RDX;

    shadow_map[REG_RSP] = SHDW_REG_RSP;
    shadow_map[REG_SPL] = SHDW_REG_SPL;

    shadow_map[REG_RBP] = SHDW_REG_RBP;
    shadow_map[REG_BPL] = SHDW_REG_BPL;

    shadow_map[REG_RSI] = SHDW_REG_RSI;
    shadow_map[REG_SIL] = SHDW_REG_SIL;

    shadow_map[REG_RDI] = SHDW_REG_RDI;
    shadow_map[REG_DIL] = SHDW_REG_DIL;

    shadow_map[REG_R8] = SHDW_REG_R8;
    shadow_map[REG_R8D] = SHDW_REG_R8D;
    shadow_map[REG_R8W] = SHDW_REG_R8W;
    shadow_map[REG_R8B] = SHDW_REG_R8B;

    shadow_map[REG_R9] = SHDW_REG_R9;
    shadow_map[REG_R9D] = SHDW_REG_R9D;
    shadow_map[REG_R9W] = SHDW_REG_R9W;
    shadow_map[REG_R9B] = SHDW_REG_R9B;

    shadow_map[REG_R10] = SHDW_REG_R10;
    shadow_map[REG_R10D] = SHDW_REG_R10D;
    shadow_map[REG_R10W] = SHDW_REG_R10W;
    shadow_map[REG_R10B] = SHDW_REG_R10B;

    shadow_map[REG_R11] = SHDW_REG_R11;
    shadow_map[REG_R11D] = SHDW_REG_R11D;
    shadow_map[REG_R11W] = SHDW_REG_R11W;
    shadow_map[REG_R11B] = SHDW_REG_R11B;

    shadow_map[REG_R12] = SHDW_REG_R12;
    shadow_map[REG_R12D] = SHDW_REG_R12D;
    shadow_map[REG_R12W] = SHDW_REG_R12W;
    shadow_map[REG_R12B] = SHDW_REG_R12B;

    shadow_map[REG_R13] = SHDW_REG_R13;
    shadow_map[REG_R13D] = SHDW_REG_R13D;
    shadow_map[REG_R13W] = SHDW_REG_R13W;
    shadow_map[REG_R13B] = SHDW_REG_R13B;

    shadow_map[REG_R14] = SHDW_REG_R14;
    shadow_map[REG_R14D] = SHDW_REG_R14D;
    shadow_map[REG_R14W] = SHDW_REG_R14W;
    shadow_map[REG_R14B] = SHDW_REG_R14B;

    shadow_map[REG_R15] = SHDW_REG_R15;
    shadow_map[REG_R15D] = SHDW_REG_R15D;
    shadow_map[REG_R15W] = SHDW_REG_R15W;
    shadow_map[REG_R15B] = SHDW_REG_R15B;

    shadow_map[REG_XMM8] = SHDW_REG_XMM8;
	shadow_map[REG_YMM8] = SHDW_REG_YMM8;
	shadow_map[REG_ZMM8] = SHDW_REG_ZMM8;

	shadow_map[REG_XMM9] = SHDW_REG_XMM9;
	shadow_map[REG_YMM9] = SHDW_REG_YMM9;
	shadow_map[REG_ZMM9] = SHDW_REG_ZMM9;

	shadow_map[REG_XMM10] = SHDW_REG_XMM10;
	shadow_map[REG_YMM10] = SHDW_REG_YMM10;
	shadow_map[REG_ZMM10] = SHDW_REG_ZMM10;

	shadow_map[REG_XMM11] = SHDW_REG_XMM11;
	shadow_map[REG_YMM11] = SHDW_REG_YMM11;
	shadow_map[REG_ZMM11] = SHDW_REG_ZMM11;

	shadow_map[REG_XMM12] = SHDW_REG_XMM12;
	shadow_map[REG_YMM12] = SHDW_REG_YMM12;
	shadow_map[REG_ZMM12] = SHDW_REG_ZMM12;

	shadow_map[REG_XMM13] = SHDW_REG_XMM13;
	shadow_map[REG_YMM13] = SHDW_REG_YMM13;
	shadow_map[REG_ZMM13] = SHDW_REG_ZMM13;

	shadow_map[REG_XMM14] = SHDW_REG_XMM14;
	shadow_map[REG_YMM14] = SHDW_REG_YMM14;
	shadow_map[REG_ZMM14] = SHDW_REG_ZMM14;

	shadow_map[REG_XMM15] = SHDW_REG_XMM15;
	shadow_map[REG_YMM15] = SHDW_REG_YMM15;
	shadow_map[REG_ZMM15] = SHDW_REG_ZMM15;

	shadow_map[REG_XMM16] = SHDW_REG_XMM16;
	shadow_map[REG_YMM16] = SHDW_REG_YMM16;
	shadow_map[REG_ZMM16] = SHDW_REG_ZMM16;

	shadow_map[REG_XMM17] = SHDW_REG_XMM17;
	shadow_map[REG_YMM17] = SHDW_REG_YMM17;
	shadow_map[REG_ZMM17] = SHDW_REG_ZMM17;

	shadow_map[REG_XMM18] = SHDW_REG_XMM18;
	shadow_map[REG_YMM18] = SHDW_REG_YMM18;
	shadow_map[REG_ZMM18] = SHDW_REG_ZMM18;

	shadow_map[REG_XMM19] = SHDW_REG_XMM19;
	shadow_map[REG_YMM19] = SHDW_REG_YMM19;
	shadow_map[REG_ZMM19] = SHDW_REG_ZMM19;

	shadow_map[REG_XMM20] = SHDW_REG_XMM20;
	shadow_map[REG_YMM20] = SHDW_REG_YMM20;
	shadow_map[REG_ZMM20] = SHDW_REG_ZMM20;

	shadow_map[REG_XMM21] = SHDW_REG_XMM21;
	shadow_map[REG_YMM21] = SHDW_REG_YMM21;
	shadow_map[REG_ZMM21] = SHDW_REG_ZMM21;

	shadow_map[REG_XMM22] = SHDW_REG_XMM22;
	shadow_map[REG_YMM22] = SHDW_REG_YMM22;
	shadow_map[REG_ZMM22] = SHDW_REG_ZMM22;

	shadow_map[REG_XMM23] = SHDW_REG_XMM23;
	shadow_map[REG_YMM23] = SHDW_REG_YMM23;
	shadow_map[REG_ZMM23] = SHDW_REG_ZMM23;

	shadow_map[REG_XMM24] = SHDW_REG_XMM24;
	shadow_map[REG_YMM24] = SHDW_REG_YMM24;
	shadow_map[REG_ZMM24] = SHDW_REG_ZMM24;

	shadow_map[REG_XMM25] = SHDW_REG_XMM25;
	shadow_map[REG_YMM25] = SHDW_REG_YMM25;
	shadow_map[REG_ZMM25] = SHDW_REG_ZMM25;

	shadow_map[REG_XMM26] = SHDW_REG_XMM26;
	shadow_map[REG_YMM26] = SHDW_REG_YMM26;
	shadow_map[REG_ZMM26] = SHDW_REG_ZMM26;

	shadow_map[REG_XMM27] = SHDW_REG_XMM27;
	shadow_map[REG_YMM27] = SHDW_REG_YMM27;
	shadow_map[REG_ZMM27] = SHDW_REG_ZMM27;

	shadow_map[REG_XMM28] = SHDW_REG_XMM28;
	shadow_map[REG_YMM28] = SHDW_REG_YMM28;
	shadow_map[REG_ZMM28] = SHDW_REG_ZMM28;

	shadow_map[REG_XMM29] = SHDW_REG_XMM29;
	shadow_map[REG_YMM29] = SHDW_REG_YMM29;
	shadow_map[REG_ZMM29] = SHDW_REG_ZMM29;

	shadow_map[REG_XMM30] = SHDW_REG_XMM30;
	shadow_map[REG_YMM30] = SHDW_REG_YMM30;
	shadow_map[REG_ZMM30] = SHDW_REG_ZMM30;

	shadow_map[REG_XMM31] = SHDW_REG_XMM31;
	shadow_map[REG_YMM31] = SHDW_REG_YMM31;
	shadow_map[REG_ZMM31] = SHDW_REG_ZMM31;

#endif

    shadow_map[REG_EIP] = SHDW_REG_EIP;

    shadow_map[REG_EAX] = SHDW_REG_EAX;
    shadow_map[REG_AX] = SHDW_REG_AX;
    shadow_map[REG_AL] = SHDW_REG_AL;
    shadow_map[REG_AH] = SHDW_REG_AH;

    shadow_map[REG_EBX] = SHDW_REG_EBX;
    shadow_map[REG_BX] = SHDW_REG_BX;
    shadow_map[REG_BL] = SHDW_REG_BL;
    shadow_map[REG_BH] = SHDW_REG_BH;

    shadow_map[REG_ECX] = SHDW_REG_ECX;
    shadow_map[REG_CX] = SHDW_REG_CX;
    shadow_map[REG_CL] = SHDW_REG_CL;
    shadow_map[REG_CH] = SHDW_REG_CH;

    shadow_map[REG_EDX] = SHDW_REG_EDX;
    shadow_map[REG_DX] = SHDW_REG_DX;
    shadow_map[REG_DL] = SHDW_REG_DL;
    shadow_map[REG_DH] = SHDW_REG_DH;

    shadow_map[REG_ESP] = SHDW_REG_ESP;
    shadow_map[REG_SP] = SHDW_REG_SP;

    shadow_map[REG_EBP] = SHDW_REG_EBP;
    shadow_map[REG_BP] = SHDW_REG_BP;

    shadow_map[REG_ESI] = SHDW_REG_ESI;
    shadow_map[REG_SI] = SHDW_REG_SI;
    
    shadow_map[REG_EDI] = SHDW_REG_EDI;
    shadow_map[REG_DI] = SHDW_REG_DI;


    shadow_map[REG_XMM0] = SHDW_REG_XMM0;
	shadow_map[REG_YMM0] = SHDW_REG_YMM0;
	shadow_map[REG_ZMM0] = SHDW_REG_ZMM0;

	shadow_map[REG_XMM1] = SHDW_REG_XMM1;
	shadow_map[REG_YMM1] = SHDW_REG_YMM1;
	shadow_map[REG_ZMM1] = SHDW_REG_ZMM1;

	shadow_map[REG_XMM2] = SHDW_REG_XMM2;
	shadow_map[REG_YMM2] = SHDW_REG_YMM2;
	shadow_map[REG_ZMM2] = SHDW_REG_ZMM2;

	shadow_map[REG_XMM3] = SHDW_REG_XMM3;
	shadow_map[REG_YMM3] = SHDW_REG_YMM3;
	shadow_map[REG_ZMM3] = SHDW_REG_ZMM3;

	shadow_map[REG_XMM4] = SHDW_REG_XMM4;
	shadow_map[REG_YMM4] = SHDW_REG_YMM4;
	shadow_map[REG_ZMM4] = SHDW_REG_ZMM4;

	shadow_map[REG_XMM5] = SHDW_REG_XMM5;
	shadow_map[REG_YMM5] = SHDW_REG_YMM5;
	shadow_map[REG_ZMM5] = SHDW_REG_ZMM5;

	shadow_map[REG_XMM6] = SHDW_REG_XMM6;
	shadow_map[REG_YMM6] = SHDW_REG_YMM6;
	shadow_map[REG_ZMM6] = SHDW_REG_ZMM6;

	shadow_map[REG_XMM7] = SHDW_REG_XMM7;
	shadow_map[REG_YMM7] = SHDW_REG_YMM7;
	shadow_map[REG_ZMM7] = SHDW_REG_ZMM7;

    shadow_map[REG_MM0] = SHDW_REG_MM0;

    shadow_map[REG_MM1] = SHDW_REG_MM1;

    shadow_map[REG_MM2] = SHDW_REG_MM2;

    shadow_map[REG_MM3] = SHDW_REG_MM3;

    shadow_map[REG_MM4] = SHDW_REG_MM4;

    shadow_map[REG_MM5] = SHDW_REG_MM5;

    shadow_map[REG_MM6] = SHDW_REG_MM6;

    shadow_map[REG_MM7] = SHDW_REG_MM7;

    shadow_map[REG_ST0] = SHDW_REG_ST0;

    shadow_map[REG_ST1] = SHDW_REG_ST1;

    shadow_map[REG_ST2] = SHDW_REG_ST2;

    shadow_map[REG_ST3] = SHDW_REG_ST3;

    shadow_map[REG_ST4] = SHDW_REG_ST4;

    shadow_map[REG_ST5] = SHDW_REG_ST5;

    shadow_map[REG_ST6] = SHDW_REG_ST6;

    shadow_map[REG_ST7] = SHDW_REG_ST7;

    shadow_map[REG_K0] = SHDW_REG_K0;

    shadow_map[REG_K1] = SHDW_REG_K1;

    shadow_map[REG_K2] = SHDW_REG_K2;

    shadow_map[REG_K3] = SHDW_REG_K3;

    shadow_map[REG_K4] = SHDW_REG_K4;

    shadow_map[REG_K5] = SHDW_REG_K5;

    shadow_map[REG_K6] = SHDW_REG_K6;

    shadow_map[REG_K7] = SHDW_REG_K7;
}