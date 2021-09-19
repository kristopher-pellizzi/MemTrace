#include "Registers.h"

#include <iostream>

static map<int, int> REGS;
static bool mapIsInitialized = false;

#define X(v) + 1
static int newRegister = 0 REG_TABLE(X);
#undef X

#define X(v) #v, 
static const char* regNames[] = {
    REG_TABLE(X)
};
#undef X

static void test_regs(){
    std::cout << "Test registers: " <<std::endl;
    int reg = get_normalized_register(REG_RAX);
    std::cout << "RAX: " << REG_StringShort(REG_RAX) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_EAX);
    std::cout << "EAX: " << REG_StringShort(REG_EAX) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_AX);
    std::cout << "AX: " << REG_StringShort(REG_AX) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_AL);
    std::cout << "AL: " << REG_StringShort(REG_AL) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;
    
    reg = get_normalized_register(REG_AH);
    std::cout << "AH: " << REG_StringShort(REG_AH) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_R8);
    std::cout << "R8: " << REG_StringShort(REG_R8) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_R8D);
    std::cout << "R8D: " << REG_StringShort(REG_R8D) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_R8W);
    std::cout << "R8W: " << REG_StringShort(REG_R8W) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

    reg = get_normalized_register(REG_R8B);
    std::cout << "R8B: " << REG_StringShort(REG_R8B) << "; " << get_normalized_register_name(reg) << "(" << std::dec << reg << ")" <<std::endl;

}

void init_regs_map(){

// According to Intel PIN's register definitions in file reg_ia32.PH, these registers are defined only
// in a 64 bit machine. In that case, TARGET_IA32E is defined
#ifdef TARGET_IA32E
    REGS[REG_RAX] = NORM_REG_RAX;
    
    REGS[REG_RBX] = NORM_REG_RBX;

    REGS[REG_RCX] = NORM_REG_RCX;

    REGS[REG_RDX] = NORM_REG_RDX;

    REGS[REG_RSP] = NORM_REG_RSP;
    REGS[REG_SPL] = NORM_REG_RSP;

    REGS[REG_RBP] = NORM_REG_RBP;
    REGS[REG_BPL] = NORM_REG_RBP;

    REGS[REG_RSI] = NORM_REG_RSI;
    REGS[REG_SIL] = NORM_REG_RSI;

    REGS[REG_RDI] = NORM_REG_RDI;
    REGS[REG_DIL] = NORM_REG_RDI;

    REGS[REG_R8] = NORM_REG_R8;
    REGS[REG_R8D] = NORM_REG_R8;
    REGS[REG_R8W] = NORM_REG_R8;
    REGS[REG_R8B] = NORM_REG_R8;

    REGS[REG_R9] = NORM_REG_R9;
    REGS[REG_R9D] = NORM_REG_R9;
    REGS[REG_R9W] = NORM_REG_R9;
    REGS[REG_R9B] = NORM_REG_R9;

    REGS[REG_R10] = NORM_REG_R10;
    REGS[REG_R10D] = NORM_REG_R10;
    REGS[REG_R10W] = NORM_REG_R10;
    REGS[REG_R10B] = NORM_REG_R10;

    REGS[REG_R11] = NORM_REG_R11;
    REGS[REG_R11D] = NORM_REG_R11;
    REGS[REG_R11W] = NORM_REG_R11;
    REGS[REG_R11B] = NORM_REG_R11;

    REGS[REG_R12] = NORM_REG_R12;
    REGS[REG_R12D] = NORM_REG_R12;
    REGS[REG_R12W] = NORM_REG_R12;
    REGS[REG_R12B] = NORM_REG_R12;

    REGS[REG_R13] = NORM_REG_R13;
    REGS[REG_R13D] = NORM_REG_R13;
    REGS[REG_R13W] = NORM_REG_R13;
    REGS[REG_R13B] = NORM_REG_R13;

    REGS[REG_R14] = NORM_REG_R14;
    REGS[REG_R14D] = NORM_REG_R14;
    REGS[REG_R14W] = NORM_REG_R14;
    REGS[REG_R14B] = NORM_REG_R14;

    REGS[REG_R15] = NORM_REG_R15;
    REGS[REG_R15D] = NORM_REG_R15;
    REGS[REG_R15W] = NORM_REG_R15;
    REGS[REG_R15B] = NORM_REG_R15;

    REGS[REG_XMM8] = NORM_REG_XMM8;
    REGS[REG_YMM8] = NORM_REG_XMM8;
    REGS[REG_ZMM8] = NORM_REG_XMM8;

    REGS[REG_XMM9] = NORM_REG_XMM9;
    REGS[REG_YMM9] = NORM_REG_XMM9;
    REGS[REG_ZMM9] = NORM_REG_XMM9;

    REGS[REG_XMM10] = NORM_REG_XMM10;
    REGS[REG_YMM10] = NORM_REG_XMM10;
    REGS[REG_ZMM10] = NORM_REG_XMM10;

    REGS[REG_XMM11] = NORM_REG_XMM11;
    REGS[REG_YMM11] = NORM_REG_XMM11;
    REGS[REG_ZMM11] = NORM_REG_XMM11;

    REGS[REG_XMM12] = NORM_REG_XMM12;
    REGS[REG_YMM12] = NORM_REG_XMM12;
    REGS[REG_ZMM12] = NORM_REG_XMM12;

    REGS[REG_XMM13] = NORM_REG_XMM13;
    REGS[REG_YMM13] = NORM_REG_XMM13;
    REGS[REG_ZMM13] = NORM_REG_XMM13;

    REGS[REG_XMM14] = NORM_REG_XMM14;
    REGS[REG_YMM14] = NORM_REG_XMM14;
    REGS[REG_ZMM14] = NORM_REG_XMM14;

    REGS[REG_XMM15] = NORM_REG_XMM15;
    REGS[REG_YMM15] = NORM_REG_XMM15;
    REGS[REG_ZMM15] = NORM_REG_XMM15;

    REGS[REG_XMM16] = NORM_REG_XMM16;
    REGS[REG_YMM16] = NORM_REG_XMM16;
    REGS[REG_ZMM16] = NORM_REG_XMM16;

    REGS[REG_XMM17] = NORM_REG_XMM17;
    REGS[REG_YMM17] = NORM_REG_XMM17;
    REGS[REG_ZMM17] = NORM_REG_XMM17;

    REGS[REG_XMM18] = NORM_REG_XMM18;
    REGS[REG_YMM18] = NORM_REG_XMM18;
    REGS[REG_ZMM18] = NORM_REG_XMM18;

    REGS[REG_XMM19] = NORM_REG_XMM19;
    REGS[REG_YMM19] = NORM_REG_XMM19;
    REGS[REG_ZMM19] = NORM_REG_XMM19;

    REGS[REG_XMM20] = NORM_REG_XMM20;
    REGS[REG_YMM20] = NORM_REG_XMM20;
    REGS[REG_ZMM20] = NORM_REG_XMM20;

    REGS[REG_XMM21] = NORM_REG_XMM21;
    REGS[REG_YMM21] = NORM_REG_XMM21;
    REGS[REG_ZMM21] = NORM_REG_XMM21;

    REGS[REG_XMM22] = NORM_REG_XMM22;
    REGS[REG_YMM22] = NORM_REG_XMM22;
    REGS[REG_ZMM22] = NORM_REG_XMM22;

    REGS[REG_XMM23] = NORM_REG_XMM23;
    REGS[REG_YMM23] = NORM_REG_XMM23;
    REGS[REG_ZMM23] = NORM_REG_XMM23;

    REGS[REG_XMM24] = NORM_REG_XMM24;
    REGS[REG_YMM24] = NORM_REG_XMM24;
    REGS[REG_ZMM24] = NORM_REG_XMM24;

    REGS[REG_XMM25] = NORM_REG_XMM25;
    REGS[REG_YMM25] = NORM_REG_XMM25;
    REGS[REG_ZMM25] = NORM_REG_XMM25;

    REGS[REG_XMM26] = NORM_REG_XMM26;
    REGS[REG_YMM26] = NORM_REG_XMM26;
    REGS[REG_ZMM26] = NORM_REG_XMM26;

    REGS[REG_XMM27] = NORM_REG_XMM27;
    REGS[REG_YMM27] = NORM_REG_XMM27;
    REGS[REG_ZMM27] = NORM_REG_XMM27;

    REGS[REG_XMM28] = NORM_REG_XMM28;
    REGS[REG_YMM28] = NORM_REG_XMM28;
    REGS[REG_ZMM28] = NORM_REG_XMM28;

    REGS[REG_XMM29] = NORM_REG_XMM29;
    REGS[REG_YMM29] = NORM_REG_XMM29;
    REGS[REG_ZMM29] = NORM_REG_XMM29;

    REGS[REG_XMM30] = NORM_REG_XMM30;
    REGS[REG_YMM30] = NORM_REG_XMM30;
    REGS[REG_ZMM30] = NORM_REG_XMM30;

    REGS[REG_XMM31] = NORM_REG_XMM31;
    REGS[REG_YMM31] = NORM_REG_XMM31;
    REGS[REG_ZMM31] = NORM_REG_XMM31;

#endif

    REGS[REG_EAX] = NORM_REG_RAX;
    REGS[REG_AX] = NORM_REG_RAX;
    REGS[REG_AL] = NORM_REG_RAX;
    REGS[REG_AH] = NORM_REG_RAX;

    REGS[REG_EBX] = NORM_REG_RBX;
    REGS[REG_BX] = NORM_REG_RBX;
    REGS[REG_BL] = NORM_REG_RBX;
    REGS[REG_BH] = NORM_REG_RBX;

    REGS[REG_ECX] = NORM_REG_RCX;
    REGS[REG_CX] = NORM_REG_RCX;
    REGS[REG_CL] = NORM_REG_RCX;
    REGS[REG_CH] = NORM_REG_RCX;

    REGS[REG_EDX] = NORM_REG_RDX;
    REGS[REG_DX] = NORM_REG_RDX;
    REGS[REG_DL] = NORM_REG_RDX;
    REGS[REG_DH] = NORM_REG_RDX;

    REGS[REG_ESP] = NORM_REG_RSP;
    REGS[REG_SP] = NORM_REG_RSP;

    REGS[REG_EBP] = NORM_REG_RBP;
    REGS[REG_BP] = NORM_REG_RBP;

    REGS[REG_ESI] = NORM_REG_RSI;
    REGS[REG_SI] = NORM_REG_RSI;
    
    REGS[REG_EDI] = NORM_REG_RDI;
    REGS[REG_DI] = NORM_REG_RDI;


    REGS[REG_XMM0] = NORM_REG_XMM0;
    REGS[REG_YMM0] = NORM_REG_XMM0;
    REGS[REG_ZMM0] = NORM_REG_XMM0;

    REGS[REG_XMM1] = NORM_REG_XMM1;
    REGS[REG_YMM1] = NORM_REG_XMM1;
    REGS[REG_ZMM1] = NORM_REG_XMM1;

    REGS[REG_XMM2] = NORM_REG_XMM2;
    REGS[REG_YMM2] = NORM_REG_XMM2;
    REGS[REG_ZMM2] = NORM_REG_XMM2;

    REGS[REG_XMM3] = NORM_REG_XMM3;
    REGS[REG_YMM3] = NORM_REG_XMM3;
    REGS[REG_ZMM3] = NORM_REG_XMM3;

    REGS[REG_XMM4] = NORM_REG_XMM4;
    REGS[REG_YMM4] = NORM_REG_XMM4;
    REGS[REG_ZMM4] = NORM_REG_XMM4;

    REGS[REG_XMM5] = NORM_REG_XMM5;
    REGS[REG_YMM5] = NORM_REG_XMM5;
    REGS[REG_ZMM5] = NORM_REG_XMM5;

    REGS[REG_XMM6] = NORM_REG_XMM6;
    REGS[REG_YMM6] = NORM_REG_XMM6;
    REGS[REG_ZMM6] = NORM_REG_XMM6;

    REGS[REG_XMM7] = NORM_REG_XMM7;
    REGS[REG_YMM7] = NORM_REG_XMM7;
    REGS[REG_ZMM7] = NORM_REG_XMM7;

    REGS[REG_MM0] = NORM_REG_MMX0;

    REGS[REG_MM1] = NORM_REG_MMX1;

    REGS[REG_MM2] = NORM_REG_MMX2;

    REGS[REG_MM3] = NORM_REG_MMX3;

    REGS[REG_MM4] = NORM_REG_MMX4;

    REGS[REG_MM5] = NORM_REG_MMX5;

    REGS[REG_MM6] = NORM_REG_MMX6;

    REGS[REG_MM7] = NORM_REG_MMX7;

    REGS[REG_ST0] = NORM_REG_ST0;

    REGS[REG_ST1] = NORM_REG_ST1;

    REGS[REG_ST2] = NORM_REG_ST2;

    REGS[REG_ST3] = NORM_REG_ST3;

    REGS[REG_ST4] = NORM_REG_ST4;

    REGS[REG_ST5] = NORM_REG_ST5;

    REGS[REG_ST6] = NORM_REG_ST6;

    REGS[REG_ST7] = NORM_REG_ST7;

    REGS[REG_K0] = NORM_REG_K0;

    REGS[REG_K1] = NORM_REG_K1;

    REGS[REG_K2] = NORM_REG_K2;

    REGS[REG_K3] = NORM_REG_K3;

    REGS[REG_K4] = NORM_REG_K4;

    REGS[REG_K5] = NORM_REG_K5;

    REGS[REG_K6] = NORM_REG_K6;

    REGS[REG_K7] = NORM_REG_K7;

    mapIsInitialized = true;

    // Function used to test register mappings
    // test_regs();
}

int get_normalized_register(int pinReg){
    if(!mapIsInitialized)
        init_regs_map();
    auto iter = REGS.find(pinReg);
    // If there's a normalized version of the register, return it...
    if(iter != REGS.end())
        return iter->second;

    // ... otherwise just create a new ad-hoc register and return it
    REGS[pinReg] = newRegister;
    return newRegister++;
}

const char* get_normalized_register_name(int reg){
    return regNames[reg];
}