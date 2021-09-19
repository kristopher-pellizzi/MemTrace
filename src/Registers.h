#include "pin.H"
#include <map>

using std::map;

/*
    This header file is designed in order to map all the aliasing registers (e.g. rax, eax, ax, al, ah) into the same 
    normalized register.
    This is done because the small taint analysis performed on registers require to know if any register is overwritten 
    by any instruction.
    By using Intel PIN registers, aliasing registers will be distinguished as different registers, so overwrites or reads 
    happening by using aliasing registers won't be detected, thus potentially leading to false positives and false negatives.

    Of course, the taint analysis is very simple and minimalistic. It does not keep track of the uninitialized bytes in a 
    register, but simply a boolean value specifying whether it has a value read from an uninitialized read or not.
    Moreover, sometimes writing an aliasing register does not completely overwrite the full size register.
    For instance, let's say we execute the following code snippet:

    =====================================================

        mov $0xffffffffffffffff, %r8    => Writes the whole register
        mov $3, %r8d                    => Writes the whole register    
        mov $0xffffffffffffffff, %r8    => Writes the whole register
        mov $5, %r8w                    => Writes only the 2 Least Significant Bytes
        mov $0xffffffffffffffff, %r8    => Writes the whole register
        mov $4, %r8b                    => Writes only the Least Significant Byte

    =====================================================

    In order to keep track of this specific behavior, we would need to perform a more complex taint analysis, which 
    keeps track of uninitialized bytes in a set of shadow registers.
    While this is feasible, it will also increase complexity and execution time of the analysis and is left as a 
    future work.
*/

/*
    The following macro has 2 purposes:
        1) Create the enumeration defined below
        2) Allow to declare an int variable in Registers.cpp containing the number of elements in the enumeration

    While the first could simply be done by manually create the enum, the second one was not possible without the usage 
    of this X-Macro. The only alternative would have been to hard-code the number of elements, but could be
    dangerous if any change is required to the enum.

    This way, any change done to macro REG_TABLE will be reflected both in the enumeration and the counter.
    
    Note that X is another macro, defined when it is needed according to the objective (define vs count).
*/
#define REG_TABLE(X) \
    X(NORM_REG_RAX) \
    X(NORM_REG_RBX) \
    X(NORM_REG_RCX) \
    X(NORM_REG_RDX) \
    X(NORM_REG_RBP) \
    X(NORM_REG_RSP) \
    X(NORM_REG_RSI) \
    X(NORM_REG_RDI) \
    X(NORM_REG_R8) \
    X(NORM_REG_R9) \
    X(NORM_REG_R10) \
    X(NORM_REG_R11) \
    X(NORM_REG_R12) \
    X(NORM_REG_R13) \
    X(NORM_REG_R14) \
    X(NORM_REG_R15) \
    X(NORM_REG_XMM0) \
    X(NORM_REG_XMM1) \
    X(NORM_REG_XMM2) \
    X(NORM_REG_XMM3) \
    X(NORM_REG_XMM4) \
    X(NORM_REG_XMM5) \
    X(NORM_REG_XMM6) \
    X(NORM_REG_XMM7) \
    X(NORM_REG_XMM8) \
    X(NORM_REG_XMM9) \
    X(NORM_REG_XMM10) \
    X(NORM_REG_XMM11) \
    X(NORM_REG_XMM12) \
    X(NORM_REG_XMM13) \
    X(NORM_REG_XMM14) \
    X(NORM_REG_XMM15) \
    X(NORM_REG_XMM16) \
    X(NORM_REG_XMM17) \
    X(NORM_REG_XMM18) \
    X(NORM_REG_XMM19) \
    X(NORM_REG_XMM20) \
    X(NORM_REG_XMM21) \
    X(NORM_REG_XMM22) \
    X(NORM_REG_XMM23) \
    X(NORM_REG_XMM24) \
    X(NORM_REG_XMM25) \
    X(NORM_REG_XMM26) \
    X(NORM_REG_XMM27) \
    X(NORM_REG_XMM28) \
    X(NORM_REG_XMM29) \
    X(NORM_REG_XMM30) \
    X(NORM_REG_XMM31) \
    X(NORM_REG_MMX0) \
    X(NORM_REG_MMX1) \
    X(NORM_REG_MMX2) \
    X(NORM_REG_MMX3) \
    X(NORM_REG_MMX4) \
    X(NORM_REG_MMX5) \
    X(NORM_REG_MMX6) \
    X(NORM_REG_MMX7) \
    X(NORM_REG_ST0) \
    X(NORM_REG_ST1) \
    X(NORM_REG_ST2) \
    X(NORM_REG_ST3) \
    X(NORM_REG_ST4) \
    X(NORM_REG_ST5) \
    X(NORM_REG_ST6) \
    X(NORM_REG_ST7) \
    X(NORM_REG_K0) \
    X(NORM_REG_K1) \
    X(NORM_REG_K2) \
    X(NORM_REG_K3) \
    X(NORM_REG_K4) \
    X(NORM_REG_K5) \
    X(NORM_REG_K6) \
    X(NORM_REG_K7) 
        
    /* These have not been found in Intel PIN's enumeration
        NORM_REG_TMM0,
        NORM_REG_TMM1,
        NORM_REG_TMM2,
        NORM_REG_TMM3,
        NORM_REG_TMM4,
        NORM_REG_TMM5,
        NORM_REG_TMM6,
        NORM_REG_TMM7
    */

enum Registers{
    #define X(v) v, 
    REG_TABLE(X)
    #undef X   
};

void init_regs_map();

int get_normalized_register(int pinReg);