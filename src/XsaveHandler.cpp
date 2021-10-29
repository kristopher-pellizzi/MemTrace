#include "XsaveHandler.h"
#include <iostream>

static std::ostream* out = &std::cerr;
using std::endl;

XsaveComponent::XsaveComponent() : 
    offset(0),
    size(0),
    alreadyAligned(true)
{}

XsaveComponent::XsaveComponent(uint32_t offset, uint32_t size, bool alreadyAligned) :
    offset(offset),
    size(size),
    alreadyAligned(alreadyAligned)
{}




XsaveHandler::XsaveHandler(){
    init();
}

void XsaveHandler::init(){
    uint32_t ecxContent;

    // It is necessary to push and restore register rbx, because according to intel's manual it is a non-volatile
    // register, so the caller expects it to be untouched on return, and cpuid overwrites register rbx
    asm(
        "push %%rbx;"
        "mov $1, %%eax;"
        "cpuid;"
        "mov %%ecx, %0;"
        "pop %%rbx;"
        : "=rm"(ecxContent)
        :
        : "%ecx"
    );

    bool XsaveSupported = ((ecxContent >> 26) & 1) == 1;
    XsaveFeaturesSupported = XsaveSupported;
}

XsaveHandler& XsaveHandler::getInstance(){
    static XsaveHandler instance;

    return instance;
}

XsaveComponent XsaveHandler::getComponentInfo(uint32_t compNum){

    switch(compNum){
        case 0:
            return XsaveComponent(32, 128, true);
        
        case 1:
            return XsaveComponent(160, 256, true);

        default: 
        {
            uint32_t offset;
            uint32_t size;
            uint32_t alignment;

            asm(
                "push %%rbx;"
                "mov $0x0d, %%eax;"
                "mov %3, %%ecx;"
                "cpuid;"
                "mov %%eax, %1;"
                "mov %%ebx, %0;"
                "mov %%ecx, %2;"
                "pop %%rbx;"
                : "=rm"(offset), "=rm"(size), "=rm"(alignment)
                : "r"(compNum)
                : "%eax", "%ebx"
            );

            XsaveComponent ret;
            ret.offset = offset;
            ret.size = size;
            alignment = (alignment >> 1) & 1;

            if(alignment == 0)
                ret.alreadyAligned = true;
            else
                ret.alreadyAligned = false;


            return ret;
        }
    }
}


uint32_t XsaveHandler::getXcr0Bitmap(){
    uint32_t ret;

    asm(
        "xor %%ecx, %%ecx;"
        "xgetbv;"
        "mov %%eax, %0;"
        : "=rm"(ret)
        :
        : "%eax", "%edx"
    );

    return ret;
}

bool XsaveHandler::isX87Stored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap;
    tmp &= 1;
    return tmp == 1;

}

bool XsaveHandler::isXmmStored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap; 
    tmp = (tmp >> 1) & 1;
    return tmp == 1;
}

bool XsaveHandler::isYmmStored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap;
    tmp = (tmp >> YMMCompNum) & 1;
    return tmp == 1;
}

bool XsaveHandler::isZmmHighStored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap;
    tmp = (tmp >> ZMMHighCompNum) & 1;
    return tmp == 1;
}

bool XsaveHandler::isZmmFullStored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap;
    tmp = (tmp >> ZMMFullCompNum) & 1;
    return tmp == 1;
}

bool XsaveHandler::isKmaskStored(uint32_t stateComponentBitmap){
    uint32_t tmp = stateComponentBitmap;
    tmp = (tmp >> KregsCompNum) & 1;
    return tmp == 1;
}

set<AnalysisArgs> XsaveHandler::getXsaveAnalysisArgs(uint32_t eaxContent, OPCODE opcode, ADDRINT addr, UINT32 size){
    if(!XsaveFeaturesSupported){
        *out << "Error: call to XSAVE on a processor not supporting XSAVE features" << endl;
        exit(EXIT_FAILURE);
    }
    set<AnalysisArgs> ret;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    uint32_t xcr0Content = getXcr0Bitmap();
    uint32_t stateComponentBitmap = xcr0Content & eaxContent;

    if(isX87Stored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 16;

        for(unsigned i = 0; i < 8; ++i, storeAddr += storeSize){
            REG reg = x87Regs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent X87Info = getComponentInfo(X87CompNum);
                    storeAddr = addr + X87Info.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
           
    }

    if(isXmmStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 16;

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += storeSize){
            REG reg = xmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent XMMInfo = getComponentInfo(XMMCompNum);
                    storeAddr = addr + XMMInfo.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }

    // If it is a call to FXSAVE, we need to store only ST and XMM registers, so return
    if(opcode == XED_ICLASS_FXSAVE || opcode == XED_ICLASS_FXSAVE64){
        return ret;
    }

    if(isYmmStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        // Only the high 128 bits are stored here, as the low 128 ones have already been stored
        // by storing XMM registers
        UINT32 storeSize = 16;

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += storeSize){
            REG reg = ymmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent YMMInfo = getComponentInfo(YMMCompNum);
                    storeAddr = addr + YMMInfo.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }

    if(isZmmHighStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 32;

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += storeSize){
            REG reg = zmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent ZMMHighInfo = getComponentInfo(ZMMHighCompNum);
                    storeAddr = addr + ZMMHighInfo.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }


    if(isZmmFullStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 64;

        for(unsigned i = regsNum; i < ZmmMaxNum; ++i, storeAddr += storeSize){
            REG reg = zmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent ZMMFullInfo = getComponentInfo(ZMMFullCompNum);
                    storeAddr = addr + ZMMFullInfo.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }


    if(isKmaskStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 8;

        for(unsigned i = 0; i < 8; ++i, storeAddr += storeSize){
            REG reg = kRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent KregsInfo = getComponentInfo(KregsCompNum);
                    storeAddr = addr + KregsInfo.offset + storeSize * i;
                    infoAlreadyInitialized = true;
                }

                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }

    return ret;
}



set<AnalysisArgs> XsaveHandler::getXrstorAnalysisArgs(uint32_t eaxContent, OPCODE opcode, ADDRINT addr, UINT32 size){
    if(!XsaveFeaturesSupported){
        *out << "Error: call to XRSTOR on a processor not supporting XSAVE features" << endl;
        exit(EXIT_FAILURE);
    }
    
    set<AnalysisArgs> ret;
    uint32_t xcr0Content = getXcr0Bitmap();
    uint32_t stateComponentBitmap = xcr0Content & eaxContent;
    /*
        XSTATE_BV is in the XSAVE header, which is stored at offset 512 from the base of the XSAVE area.
        It is a 64 bits value, but all components we are interested in are mapped into the 32 least significant bits, so, we 
        only take them to be AND-ed with the current |stateComponentBitmap|
    */
    uint32_t xstate_bv_low32;
    PIN_SafeCopy(&xstate_bv_low32, (void*) (addr + 512), 4);
    uint32_t to_be_restored = stateComponentBitmap & xstate_bv_low32;
    uint32_t to_be_initialized = stateComponentBitmap & (~ xstate_bv_low32);

    if(isX87Stored(to_be_restored)){
        XsaveComponent X87Info = getComponentInfo(X87CompNum);
        ADDRINT loadAddr = addr + X87Info.offset;
        UINT32 loadSize = 16;
        // The following set is filled with registers which have been stored by a previous XSAVE in an initialized
        // state. This means they need to be reinitialized, if they are not.
        list<REG> toReinit;

        for(unsigned i = 0; i < 8; ++i, loadAddr += loadSize){
            REG reg = x87Regs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        // Reinitialize registers that were stored in an initialized state. 
        // Thos that were stored in an uninitialized state will be restored by the later call to |memtrace|
        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isX87Stored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = 0; i < 8; ++i){
            toReinit.push_back(x87Regs[i]);
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }


    if(isXmmStored(to_be_restored)){
        XsaveComponent XmmInfo = getComponentInfo(XMMCompNum);
        ADDRINT loadAddr = addr + XmmInfo.offset;
        UINT32 loadSize = 16;
        list<REG> toReinit;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = xmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isXmmStored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = 0; i < regsNum; ++i){
            toReinit.push_back(xmmRegs[i]);
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }

    if(opcode == XED_ICLASS_FXRSTOR || opcode == XED_ICLASS_FXRSTOR64){
        return ret;
    }

    if(isYmmStored(to_be_restored)){
        XsaveComponent YmmInfo = getComponentInfo(YMMCompNum);
        ADDRINT loadAddr = addr + YmmInfo.offset;
        UINT32 loadSize = 16;
        list<REG> toReinit;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = ymmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isYmmStored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = 0; i < regsNum; ++i){
            toReinit.push_back(ymmRegs[i]);
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }

    if(isZmmHighStored(to_be_restored)){
        XsaveComponent ZmmHighInfo = getComponentInfo(ZMMHighCompNum);
        ADDRINT loadAddr = addr + ZmmHighInfo.offset;
        UINT32 loadSize = 32;
        list<REG> toReinit;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = zmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isZmmHighStored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = 0; i < regsNum; ++i){
            toReinit.push_back(zmmRegs[i]);
        }
        
        InstructionHandler::getInstance().handle(&toReinit);
    }

    if(isZmmFullStored(to_be_restored)){
        XsaveComponent ZmmFullInfo = getComponentInfo(ZMMFullCompNum);
        ADDRINT loadAddr = addr + ZmmFullInfo.offset;
        UINT32 loadSize = 64;
        list<REG> toReinit;

        for(unsigned i = regsNum; i < ZmmMaxNum; ++i, loadAddr += loadSize){
            REG reg = zmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isZmmHighStored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = regsNum; i < ZmmMaxNum; ++i){
            toReinit.push_back(zmmRegs[i]);
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }

    if(isKmaskStored(to_be_restored)){
        XsaveComponent KmaskInfo = getComponentInfo(KregsCompNum);
        ADDRINT loadAddr = addr + KmaskInfo.offset;
        UINT32 loadSize = 8;
        list<REG> toReinit;

        for(unsigned i = 0; i < 8; ++i, loadAddr += loadSize){
            REG reg = kRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                list<REG>* regs = new list<REG>();
                regs->push_back(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
            else{
                toReinit.push_back(reg);
            }
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }
    else if(isKmaskStored(to_be_initialized)){
        list<REG> toReinit;
        for(unsigned i = 0; i < 8; ++i){
            toReinit.push_back(kRegs[i]);
        }

        InstructionHandler::getInstance().handle(&toReinit);
    }

    return ret;
}