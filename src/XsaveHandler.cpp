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

    asm(
        "mov $1, %%eax;"
        "cpuid;"
        "mov %%ecx, %0;"
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
                "mov $0x0d, %%eax;"
                "mov %3, %%ecx;"
                "cpuid;"
                "mov %%eax, %1;"
                "mov %%ebx, %0;"
                "mov %%ecx, %2;"
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

        for(unsigned i = 0; i < 8; ++i, storeAddr += 16){
            REG reg = x87Regs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent X87Info = getComponentInfo(X87CompNum);
                    storeAddr = addr + X87Info.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
           
    }

    if(isXmmStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 16;

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += 16){
            REG reg = xmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent XMMInfo = getComponentInfo(XMMCompNum);
                    storeAddr = addr + XMMInfo.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

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

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += 16){
            REG reg = ymmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent YMMInfo = getComponentInfo(YMMCompNum);
                    storeAddr = addr + YMMInfo.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }

    if(isZmmHighStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 32;

        for(unsigned i = 0; i < regsNum; ++i, storeAddr += 32){
            REG reg = zmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent ZMMHighInfo = getComponentInfo(ZMMHighCompNum);
                    storeAddr = addr + ZMMHighInfo.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }


    if(isZmmFullStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 64;

        for(unsigned i = regsNum; i < ZmmMaxNum; ++i, storeAddr += 64){
            REG reg = zmmRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent ZMMFullInfo = getComponentInfo(ZMMFullCompNum);
                    storeAddr = addr + ZMMFullInfo.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, storeAddr, storeSize);
                ret.insert(args);
            }
        }
    }


    if(isKmaskStored(stateComponentBitmap)){
        bool infoAlreadyInitialized = false;
        ADDRINT storeAddr = 0;
        UINT32 storeSize = 8;

        for(unsigned i = 0; i < 8; ++i, storeAddr +=8){
            REG reg = kRegs[i];
            if(registerFile.isUninitialized(reg)){
                if(!infoAlreadyInitialized){
                    XsaveComponent KregsInfo = getComponentInfo(KregsCompNum);
                    storeAddr = addr + KregsInfo.offset;
                    infoAlreadyInitialized = true;
                }

                set<REG>* regs = new set<REG>();
                regs->insert(reg);

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
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
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

        for(unsigned i = 0; i < 8; ++i, loadAddr += loadSize){
            REG reg = x87Regs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isX87Stored(to_be_initialized)){
        for(unsigned i = 0; i < 8; ++i){
            registerFile.setAsInitialized(x87Regs[i]);
        }
    }

    if(isXmmStored(to_be_restored)){
        XsaveComponent XmmInfo = getComponentInfo(XMMCompNum);
        ADDRINT loadAddr = addr + XmmInfo.offset;
        UINT32 loadSize = 16;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = xmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isXmmStored(to_be_initialized)){
        for(unsigned i = 0; i < regsNum; ++i){
            registerFile.setAsInitialized(xmmRegs[i]);
        }
    }

    if(opcode == XED_ICLASS_FXRSTOR || opcode == XED_ICLASS_FXRSTOR64){
        return ret;
    }

    if(isYmmStored(to_be_restored)){
        XsaveComponent YmmInfo = getComponentInfo(YMMCompNum);
        ADDRINT loadAddr = addr + YmmInfo.offset;
        UINT32 loadSize = 16;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = ymmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isYmmStored(to_be_initialized)){
        for(unsigned i = 0; i < regsNum; ++i){
            registerFile.setAsInitialized(ymmRegs[i]);
        }
    }

    if(isZmmHighStored(to_be_restored)){
        XsaveComponent ZmmHighInfo = getComponentInfo(ZMMHighCompNum);
        ADDRINT loadAddr = addr + ZmmHighInfo.offset;
        UINT32 loadSize = 32;

        for(unsigned i = 0; i < regsNum; ++i, loadAddr += loadSize){
            REG reg = zmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isZmmHighStored(to_be_initialized)){
        for(unsigned i = 0; i < regsNum; ++i){
            registerFile.setAsInitialized(zmmRegs[i]);
        }
    }

    if(isZmmFullStored(to_be_restored)){
        XsaveComponent ZmmFullInfo = getComponentInfo(ZMMFullCompNum);
        ADDRINT loadAddr = addr + ZmmFullInfo.offset;
        UINT32 loadSize = 64;

        for(unsigned i = regsNum; i < ZmmMaxNum; ++i, loadAddr += loadSize){
            REG reg = zmmRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isZmmHighStored(to_be_initialized)){
        for(unsigned i = regsNum; i < ZmmMaxNum; ++i){
            registerFile.setAsInitialized(zmmRegs[i]);
        }
    }

    if(isKmaskStored(to_be_restored)){
        XsaveComponent KmaskInfo = getComponentInfo(KregsCompNum);
        ADDRINT loadAddr = addr + KmaskInfo.offset;
        UINT32 loadSize = 8;

        for(unsigned i = 0; i < 8; ++i, loadAddr += loadSize){
            REG reg = kRegs[i];
            AccessIndex ai(loadAddr, loadSize);
            map<range_t, set<tag_t>> m = getStoredPendingReads(ai);
            if(m.size() != 0){
                set<REG>* regs = new set<REG>();
                regs->insert(reg);

                AnalysisArgs args(regs, loadAddr, loadSize);
                ret.insert(args);
            }
        }
    }
    else if(isKmaskStored(to_be_initialized)){
        for(unsigned i = 0; i < 8; ++i){
            registerFile.setAsInitialized(kRegs[i]);
        }
    }

    return ret;
}