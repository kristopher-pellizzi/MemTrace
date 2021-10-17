#include <map>
#include <set>
#include <string>
#include <sys/mman.h>
#include "Platform.h"
#include "ShadowRegister.h"
#include "misc/CeilToMultipleOf8.h"
#include "pin.H"

using std::map;
using std::set;
using std::string;

#ifndef SHDWREGISTERFILE
#define SHDWREGISTERFILE

/*
    The following macro has been designed in order to allow define some property for each register of 
    enum SHDW_REG defined below.
    It accepts, as a parameter, another macro, which MUST accept exactly 4 arguments or be variadic.
    Parameters of the called macro |X| are:
        [*] name: shadow register name/index
        [*] size: register size (in bits)
        [*] superRegister: register the considered register is a sub-register of (e.g. eax will have SHDW_REG_RAX).
            Note that registers that don't have any super-register (e.g. rax) will have their own index.
        [*] isOverwritingRegister: boolean flag (1->true; 0->false) specifying whether the register overwrites the upper bits
            of its super-register when it's written or not

    Of course, it is not necessary for the called macro to actually use all its parameters.
*/
#define REG_TABLE(X) \
    X(SHDW_REG_RAX, 64, SHDW_REG_RAX, 0) \
    X(SHDW_REG_EAX, 32, SHDW_REG_RAX, 1) \
    X(SHDW_REG_AX, 16, SHDW_REG_RAX, 0) \
    X(SHDW_REG_AH, 8, SHDW_REG_RAX, 0) \
    X(SHDW_REG_AL, 8, SHDW_REG_RAX, 0) \
    X(SHDW_REG_RBX, 64, SHDW_REG_RBX, 0) \
    X(SHDW_REG_EBX, 32, SHDW_REG_RBX, 1) \
    X(SHDW_REG_BX, 16, SHDW_REG_RBX, 0) \
    X(SHDW_REG_BH, 8, SHDW_REG_RBX, 0) \
    X(SHDW_REG_BL, 8, SHDW_REG_RBX, 0) \
    X(SHDW_REG_RCX, 64, SHDW_REG_RCX, 0) \
    X(SHDW_REG_ECX, 32, SHDW_REG_RCX, 1) \
    X(SHDW_REG_CX, 16, SHDW_REG_RCX, 0) \
    X(SHDW_REG_CH, 8, SHDW_REG_RCX, 0) \
    X(SHDW_REG_CL, 8, SHDW_REG_RCX, 0) \
    X(SHDW_REG_RDX, 64, SHDW_REG_RDX, 0) \
    X(SHDW_REG_EDX, 32, SHDW_REG_RDX, 1) \
    X(SHDW_REG_DX, 16, SHDW_REG_RDX, 0) \
    X(SHDW_REG_DH, 8, SHDW_REG_RDX, 0) \
    X(SHDW_REG_DL, 8, SHDW_REG_RDX, 0) \
    X(SHDW_REG_RSP, 64, SHDW_REG_RSP, 0) \
    X(SHDW_REG_ESP, 32, SHDW_REG_RSP, 1) \
    X(SHDW_REG_SP, 16, SHDW_REG_RSP, 0) \
    X(SHDW_REG_SPL, 8, SHDW_REG_RSP, 0) \
    X(SHDW_REG_RBP, 64, SHDW_REG_RBP, 0) \
    X(SHDW_REG_EBP, 32, SHDW_REG_RBP, 1) \
    X(SHDW_REG_BP, 16, SHDW_REG_RBP, 0) \
    X(SHDW_REG_BPL, 8, SHDW_REG_RBP, 0) \
    X(SHDW_REG_RSI, 64, SHDW_REG_RSI, 0) \
    X(SHDW_REG_ESI, 32, SHDW_REG_RSI, 1) \
    X(SHDW_REG_SI, 16, SHDW_REG_RSI, 0) \
    X(SHDW_REG_SIL, 8, SHDW_REG_RSI, 0) \
    X(SHDW_REG_RDI, 64, SHDW_REG_RDI, 0) \
    X(SHDW_REG_EDI, 32, SHDW_REG_RDI, 1) \
    X(SHDW_REG_DI, 16, SHDW_REG_RDI, 0) \
    X(SHDW_REG_DIL, 8, SHDW_REG_RDI, 0) \
    X(SHDW_REG_R8, 64, SHDW_REG_R8, 0) \
    X(SHDW_REG_R8D, 32, SHDW_REG_R8, 1) \
    X(SHDW_REG_R8W, 16, SHDW_REG_R8, 0) \
    X(SHDW_REG_R8B, 8, SHDW_REG_R8, 0) \
    X(SHDW_REG_R9, 64, SHDW_REG_R9, 0) \
    X(SHDW_REG_R9D, 32, SHDW_REG_R9, 1) \
    X(SHDW_REG_R9W, 16, SHDW_REG_R9, 0) \
    X(SHDW_REG_R9B, 8, SHDW_REG_R9, 0) \
    X(SHDW_REG_R10, 64, SHDW_REG_R10, 0) \
    X(SHDW_REG_R10D, 32, SHDW_REG_R10, 1) \
    X(SHDW_REG_R10W, 16, SHDW_REG_R10, 0) \
    X(SHDW_REG_R10B, 8, SHDW_REG_R10, 0) \
    X(SHDW_REG_R11, 64, SHDW_REG_R11, 0) \
    X(SHDW_REG_R11D, 32, SHDW_REG_R11, 1) \
    X(SHDW_REG_R11W, 16, SHDW_REG_R11, 0) \
    X(SHDW_REG_R11B, 8, SHDW_REG_R11, 0) \
    X(SHDW_REG_R12, 64, SHDW_REG_R12, 0) \
    X(SHDW_REG_R12D, 32, SHDW_REG_R12, 1) \
    X(SHDW_REG_R12W, 16, SHDW_REG_R12, 0) \
    X(SHDW_REG_R12B, 8, SHDW_REG_R12, 0) \
    X(SHDW_REG_R13, 64, SHDW_REG_R13, 0) \
    X(SHDW_REG_R13D, 32, SHDW_REG_R13, 1) \
    X(SHDW_REG_R13W, 16, SHDW_REG_R13, 0) \
    X(SHDW_REG_R13B, 8, SHDW_REG_R13, 0) \
    X(SHDW_REG_R14, 64, SHDW_REG_R14, 0) \
    X(SHDW_REG_R14D, 32, SHDW_REG_R14, 1) \
    X(SHDW_REG_R14W, 16, SHDW_REG_R14, 0) \
    X(SHDW_REG_R14B, 8, SHDW_REG_R14, 0) \
    X(SHDW_REG_R15, 64, SHDW_REG_R15, 0) \
    X(SHDW_REG_R15D, 32, SHDW_REG_R15, 1) \
    X(SHDW_REG_R15W, 16, SHDW_REG_R15, 0) \
    X(SHDW_REG_R15B, 8, SHDW_REG_R15, 0) \
    X(SHDW_REG_ZMM0, 512, SHDW_REG_ZMM0, 0) \
    X(SHDW_REG_YMM0, 256, SHDW_REG_ZMM0, 1) \
    X(SHDW_REG_XMM0, 128, SHDW_REG_ZMM0, 1) \
    X(SHDW_REG_ZMM1, 512, SHDW_REG_ZMM1, 0) \
    X(SHDW_REG_YMM1, 256, SHDW_REG_ZMM1, 1) \
    X(SHDW_REG_XMM1, 128, SHDW_REG_ZMM1, 1) \
    X(SHDW_REG_ZMM2, 512, SHDW_REG_ZMM2, 0) \
    X(SHDW_REG_YMM2, 256, SHDW_REG_ZMM2, 1) \
    X(SHDW_REG_XMM2, 128, SHDW_REG_ZMM2, 1) \
    X(SHDW_REG_ZMM3, 512, SHDW_REG_ZMM3, 0) \
    X(SHDW_REG_YMM3, 256, SHDW_REG_ZMM3, 1) \
    X(SHDW_REG_XMM3, 128, SHDW_REG_ZMM3, 1) \
    X(SHDW_REG_ZMM4, 512, SHDW_REG_ZMM4, 0) \
    X(SHDW_REG_YMM4, 256, SHDW_REG_ZMM4, 1) \
    X(SHDW_REG_XMM4, 128, SHDW_REG_ZMM4, 1) \
    X(SHDW_REG_ZMM5, 512, SHDW_REG_ZMM5, 0) \
    X(SHDW_REG_YMM5, 256, SHDW_REG_ZMM5, 1) \
    X(SHDW_REG_XMM5, 128, SHDW_REG_ZMM5, 1) \
    X(SHDW_REG_ZMM6, 512, SHDW_REG_ZMM6, 0) \
    X(SHDW_REG_YMM6, 256, SHDW_REG_ZMM6, 1) \
    X(SHDW_REG_XMM6, 128, SHDW_REG_ZMM6, 1) \
    X(SHDW_REG_ZMM7, 512, SHDW_REG_ZMM7, 0) \
    X(SHDW_REG_YMM7, 256, SHDW_REG_ZMM7, 1) \
    X(SHDW_REG_XMM7, 128, SHDW_REG_ZMM7, 1) \
    X(SHDW_REG_ZMM8, 512, SHDW_REG_ZMM8, 0) \
    X(SHDW_REG_YMM8, 256, SHDW_REG_ZMM8, 1) \
    X(SHDW_REG_XMM8, 128, SHDW_REG_ZMM8, 1) \
    X(SHDW_REG_ZMM9, 512, SHDW_REG_ZMM9, 0) \
    X(SHDW_REG_YMM9, 256, SHDW_REG_ZMM9, 1) \
    X(SHDW_REG_XMM9, 128, SHDW_REG_ZMM9, 1) \
    X(SHDW_REG_ZMM10, 512, SHDW_REG_ZMM10, 0) \
    X(SHDW_REG_YMM10, 256, SHDW_REG_ZMM10, 1) \
    X(SHDW_REG_XMM10, 128, SHDW_REG_ZMM10, 1) \
    X(SHDW_REG_ZMM11, 512, SHDW_REG_ZMM11, 0) \
    X(SHDW_REG_YMM11, 256, SHDW_REG_ZMM11, 1) \
    X(SHDW_REG_XMM11, 128, SHDW_REG_ZMM11, 1) \
    X(SHDW_REG_ZMM12, 512, SHDW_REG_ZMM12, 0) \
    X(SHDW_REG_YMM12, 256, SHDW_REG_ZMM12, 1) \
    X(SHDW_REG_XMM12, 128, SHDW_REG_ZMM12, 1) \
    X(SHDW_REG_ZMM13, 512, SHDW_REG_ZMM13, 0) \
    X(SHDW_REG_YMM13, 256, SHDW_REG_ZMM13, 1) \
    X(SHDW_REG_XMM13, 128, SHDW_REG_ZMM13, 1) \
    X(SHDW_REG_ZMM14, 512, SHDW_REG_ZMM14, 0) \
    X(SHDW_REG_YMM14, 256, SHDW_REG_ZMM14, 1) \
    X(SHDW_REG_XMM14, 128, SHDW_REG_ZMM14, 1) \
    X(SHDW_REG_ZMM15, 512, SHDW_REG_ZMM15, 0) \
    X(SHDW_REG_YMM15, 256, SHDW_REG_ZMM15, 1) \
    X(SHDW_REG_XMM15, 128, SHDW_REG_ZMM15, 1) \
    X(SHDW_REG_ZMM16, 512, SHDW_REG_ZMM16, 0) \
    X(SHDW_REG_YMM16, 256, SHDW_REG_ZMM16, 1) \
    X(SHDW_REG_XMM16, 128, SHDW_REG_ZMM16, 1) \
    X(SHDW_REG_ZMM17, 512, SHDW_REG_ZMM17, 0) \
    X(SHDW_REG_YMM17, 256, SHDW_REG_ZMM17, 1) \
    X(SHDW_REG_XMM17, 128, SHDW_REG_ZMM17, 1) \
    X(SHDW_REG_ZMM18, 512, SHDW_REG_ZMM18, 0) \
    X(SHDW_REG_YMM18, 256, SHDW_REG_ZMM18, 1) \
    X(SHDW_REG_XMM18, 128, SHDW_REG_ZMM18, 1) \
    X(SHDW_REG_ZMM19, 512, SHDW_REG_ZMM19, 0) \
    X(SHDW_REG_YMM19, 256, SHDW_REG_ZMM19, 1) \
    X(SHDW_REG_XMM19, 128, SHDW_REG_ZMM19, 1) \
    X(SHDW_REG_ZMM20, 512, SHDW_REG_ZMM20, 0) \
    X(SHDW_REG_YMM20, 256, SHDW_REG_ZMM20, 1) \
    X(SHDW_REG_XMM20, 128, SHDW_REG_ZMM20, 1) \
    X(SHDW_REG_ZMM21, 512, SHDW_REG_ZMM21, 0) \
    X(SHDW_REG_YMM21, 256, SHDW_REG_ZMM21, 1) \
    X(SHDW_REG_XMM21, 128, SHDW_REG_ZMM21, 1) \
    X(SHDW_REG_ZMM22, 512, SHDW_REG_ZMM22, 0) \
    X(SHDW_REG_YMM22, 256, SHDW_REG_ZMM22, 1) \
    X(SHDW_REG_XMM22, 128, SHDW_REG_ZMM22, 1) \
    X(SHDW_REG_ZMM23, 512, SHDW_REG_ZMM23, 0) \
    X(SHDW_REG_YMM23, 256, SHDW_REG_ZMM23, 1) \
    X(SHDW_REG_XMM23, 128, SHDW_REG_ZMM23, 1) \
    X(SHDW_REG_ZMM24, 512, SHDW_REG_ZMM24, 0) \
    X(SHDW_REG_YMM24, 256, SHDW_REG_ZMM24, 1) \
    X(SHDW_REG_XMM24, 128, SHDW_REG_ZMM24, 1) \
    X(SHDW_REG_ZMM25, 512, SHDW_REG_ZMM25, 0) \
    X(SHDW_REG_YMM25, 256, SHDW_REG_ZMM25, 1) \
    X(SHDW_REG_XMM25, 128, SHDW_REG_ZMM25, 1) \
    X(SHDW_REG_ZMM26, 512, SHDW_REG_ZMM26, 0) \
    X(SHDW_REG_YMM26, 256, SHDW_REG_ZMM26, 1) \
    X(SHDW_REG_XMM26, 128, SHDW_REG_ZMM26, 1) \
    X(SHDW_REG_ZMM27, 512, SHDW_REG_ZMM27, 0) \
    X(SHDW_REG_YMM27, 256, SHDW_REG_ZMM27, 1) \
    X(SHDW_REG_XMM27, 128, SHDW_REG_ZMM27, 1) \
    X(SHDW_REG_ZMM28, 512, SHDW_REG_ZMM28, 0) \
    X(SHDW_REG_YMM28, 256, SHDW_REG_ZMM28, 1) \
    X(SHDW_REG_XMM28, 128, SHDW_REG_ZMM28, 1) \
    X(SHDW_REG_ZMM29, 512, SHDW_REG_ZMM29, 0) \
    X(SHDW_REG_YMM29, 256, SHDW_REG_ZMM29, 1) \
    X(SHDW_REG_XMM29, 128, SHDW_REG_ZMM29, 1) \
    X(SHDW_REG_ZMM30, 512, SHDW_REG_ZMM30, 0) \
    X(SHDW_REG_YMM30, 256, SHDW_REG_ZMM30, 1) \
    X(SHDW_REG_XMM30, 128, SHDW_REG_ZMM30, 1) \
    X(SHDW_REG_ZMM31, 512, SHDW_REG_ZMM31, 0) \
    X(SHDW_REG_YMM31, 256, SHDW_REG_ZMM31, 1) \
    X(SHDW_REG_XMM31, 128, SHDW_REG_ZMM31, 1) \
    X(SHDW_REG_ST0, 80, SHDW_REG_ST0, 0) \
    X(SHDW_REG_ST1, 80, SHDW_REG_ST1, 0) \
    X(SHDW_REG_ST2, 80, SHDW_REG_ST2, 0) \
    X(SHDW_REG_ST3, 80, SHDW_REG_ST3, 0) \
    X(SHDW_REG_ST4, 80, SHDW_REG_ST4, 0) \
    X(SHDW_REG_ST5, 80, SHDW_REG_ST5, 0) \
    X(SHDW_REG_ST6, 80, SHDW_REG_ST6, 0) \
    X(SHDW_REG_ST7, 80, SHDW_REG_ST7, 0) \
    X(SHDW_REG_MM0, 64, SHDW_REG_ST0, 1) \
    X(SHDW_REG_MM1, 64, SHDW_REG_ST1, 1) \
    X(SHDW_REG_MM2, 64, SHDW_REG_ST2, 1) \
    X(SHDW_REG_MM3, 64, SHDW_REG_ST3, 1) \
    X(SHDW_REG_MM4, 64, SHDW_REG_ST4, 1) \
    X(SHDW_REG_MM5, 64, SHDW_REG_ST5, 1) \
    X(SHDW_REG_MM6, 64, SHDW_REG_ST6, 1) \
    X(SHDW_REG_MM7, 64, SHDW_REG_ST7, 1) \
    X(SHDW_REG_K0, 64, SHDW_REG_K0, 0) \
    X(SHDW_REG_K1, 64, SHDW_REG_K1, 0) \
    X(SHDW_REG_K2, 64, SHDW_REG_K2, 0) \
    X(SHDW_REG_K3, 64, SHDW_REG_K3, 0) \
    X(SHDW_REG_K4, 64, SHDW_REG_K4, 0) \
    X(SHDW_REG_K5, 64, SHDW_REG_K5, 0) \
    X(SHDW_REG_K6, 64, SHDW_REG_K6, 0) \
    X(SHDW_REG_K7, 64, SHDW_REG_K7, 0) \
    X(SHDW_REG_RIP, 64, SHDW_REG_RIP, 0) \
    X(SHDW_REG_EIP, 32, SHDW_REG_RIP, 1)


enum SHDW_REG{
    #define X(v, ...) v, 
    REG_TABLE(X)
    #undef X
};


class ShadowRegisterFile{ // Singleton
    private:
        unsigned numRegisters;
        void* shadowRegistersPtr;
        ShadowRegister** shadowRegisters;
        map<SHDW_REG, set<unsigned>> aliasingRegisters;
        set<unsigned> emptySet;
        #ifdef DEBUG
        // Set of Intel PIN's registers not having a corresponding shadow register which have already
        // been notified to the user
        set<REG> alreadyNotified;
        #endif
        string unknownString = "Unknown Register";

        /*
            The following mapping is simply useful because it numbers registers from 0 to n increasingly, allowing us to
            store pointers to the shadow registers' content into an array.
        */
        map<REG, SHDW_REG> shadow_map;

        /*
            It is possible that some Intel PIN's register have not been modeled as a shadow register because we don't really care about their content
            (e.g. EFLAGS register).
            If any of those registers are used as source/destination for program's instruction, we need to give them a unique identifier that allows us 
            to recognize them.
        */
       map<REG, unsigned> unknownRegisters;

        /*
            The following set contains all the shadow registers that have a high byte aliasing register (e.g. rax has ah)
        */
        set<SHDW_REG> haveHighByte;

        // Private methods
        ShadowRegisterFile();

        void init();
        void initMap();
        void initRequireAdjustment();
        void initShadowRegistersPtr(unsigned* sizes, SHDW_REG* superRegisters);
        void initShadowRegisters();
        ShadowRegister* getNewShadowRegister(const char* name, unsigned size, uint8_t** content);
        ShadowRegister* getNewShadowSubRegister(const char* name, unsigned size, SHDW_REG idx, SHDW_REG superRegister, bool isOverwritingReg);
        SHDW_REG convertPinReg(REG pin_reg);

    public:
        // Delete copy constructor and assignment operator
        ShadowRegisterFile(ShadowRegisterFile const& other) = delete;
        void operator=(ShadowRegisterFile const& other) = delete;

        // Destructor: de-allocate dynamically allocated shadow registers
        ~ShadowRegisterFile();

        static ShadowRegisterFile& getInstance(){
            static ShadowRegisterFile instance;

            return instance;
        }

        string& getName(REG pin_reg);
        string& getName(SHDW_REG reg);
        void setAsInitialized(REG pin_reg, uint8_t* data);
        void setAsInitialized(REG pin_reg);
        uint8_t* getContentStatus(REG pin_reg);
        uint8_t* getContentStatus(SHDW_REG reg);
        unsigned getShadowSize(REG pin_reg);
        unsigned getShadowSize(SHDW_REG reg);
        unsigned getByteSize(REG pin_reg);
        unsigned getByteSize(SHDW_REG reg);
        unsigned getShadowRegister(REG pin_reg);
        bool isUninitialized(REG pin_reg);
        bool isUninitialized(SHDW_REG reg);
        bool isUnknownRegister(REG pin_reg);
        set<unsigned>& getAliasingRegisters(REG pin_reg);
        set<unsigned>& getAliasingRegisters(SHDW_REG reg);
        set<unsigned> getCorrespondingRegisters(SHDW_REG reg, set<REG>* regSet);
        bool hasHighByte(REG pin_reg);
        bool isHighByteReg(SHDW_REG reg);



        class DecresingSizeRegisterSorter{
            public: 
                bool operator()(const unsigned x, const unsigned y);
        };

        
        class IncreasingSizeRegisterSorter{
            public:
                bool operator()(const unsigned x, const unsigned y);
        };
};

#endif // SHDWREGISTERFILE