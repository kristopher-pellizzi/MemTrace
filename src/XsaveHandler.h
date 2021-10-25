#include <set>
#include "pin.H"
#include "AnalysisArgs.h"
#include "ShadowRegisterFile.h"
#include "misc/PendingReads.h"

#ifndef XSAVEHANDLER
#define XSAVEHANDLER

using std::set;
using std::pair;

struct XsaveComponent{
    uint32_t offset;
    uint32_t size;
    bool alreadyAligned;

    XsaveComponent();
    XsaveComponent(uint32_t offset, uint32_t size, bool alreadyAligned);
};





class XsaveHandler{
    private:
        XsaveHandler();

        #ifdef TARGET_IA32E
            #define XMM_NUM 32
        #else  
            #define XMM_NUM 8
        #endif


        unsigned regsNum = XMM_NUM == 8 ? XMM_NUM : 16;
        unsigned ZmmMaxNum = 32;

        REG x87Regs[8] = {
            REG_ST0,
            REG_ST1,
            REG_ST2,
            REG_ST3,
            REG_ST4,
            REG_ST5,
            REG_ST6,
            REG_ST7
        };
 
        REG xmmRegs[XMM_NUM] = {
            REG_XMM0,
            REG_XMM1,
            REG_XMM2,
            REG_XMM3,
            REG_XMM4,
            REG_XMM5,
            REG_XMM6,
            REG_XMM7,
        #ifdef TARGET_IA32E
            REG_XMM8,
            REG_XMM9,
            REG_XMM10,
            REG_XMM11,
            REG_XMM12,
            REG_XMM13,
            REG_XMM14,
            REG_XMM15,
            REG_XMM16,
            REG_XMM17,
            REG_XMM18,
            REG_XMM19,
            REG_XMM20,
            REG_XMM21,
            REG_XMM22,
            REG_XMM23,
            REG_XMM24,
            REG_XMM25,
            REG_XMM26,
            REG_XMM27,
            REG_XMM28,
            REG_XMM29,
            REG_XMM30,
            REG_XMM31
        #endif
        };

        REG ymmRegs[XMM_NUM] = {
            REG_YMM0,
            REG_YMM1,
            REG_YMM2,
            REG_YMM3,
            REG_YMM4,
            REG_YMM5,
            REG_YMM6,
            REG_YMM7,
        #ifdef TARGET_IA32E
            REG_YMM8,
            REG_YMM9,
            REG_YMM10,
            REG_YMM11,
            REG_YMM12,
            REG_YMM13,
            REG_YMM14,
            REG_YMM15,
            REG_YMM16,
            REG_YMM17,
            REG_YMM18,
            REG_YMM19,
            REG_YMM20,
            REG_YMM21,
            REG_YMM22,
            REG_YMM23,
            REG_YMM24,
            REG_YMM25,
            REG_YMM26,
            REG_YMM27,
            REG_YMM28,
            REG_YMM29,
            REG_YMM30,
            REG_YMM31
        #endif
        };

        REG kRegs[8] = {
            REG_K0,
            REG_K1,
            REG_K2,
            REG_K3,
            REG_K4,
            REG_K5,
            REG_K6,
            REG_K7
        };

        REG zmmRegs[XMM_NUM] = {
            REG_ZMM0,
            REG_ZMM1,
            REG_ZMM2,
            REG_ZMM3,
            REG_ZMM4,
            REG_ZMM5,
            REG_ZMM6,
            REG_ZMM7,
        #ifdef TARGET_IA32E
            REG_ZMM8,
            REG_ZMM9,
            REG_ZMM10,
            REG_ZMM11,
            REG_ZMM12,
            REG_ZMM13,
            REG_ZMM14,
            REG_ZMM15,
            REG_ZMM16,
            REG_ZMM17,
            REG_ZMM18,
            REG_ZMM19,
            REG_ZMM20,
            REG_ZMM21,
            REG_ZMM22,
            REG_ZMM23,
            REG_ZMM24,
            REG_ZMM25,
            REG_ZMM26,
            REG_ZMM27,
            REG_ZMM28,
            REG_ZMM29,
            REG_ZMM30,
            REG_ZMM31
        #endif
        };


        uint32_t X87CompNum = 0;
        uint32_t XMMCompNum = 1;
        uint32_t YMMCompNum = 2;
        uint32_t ZMMHighCompNum = 6;
        uint32_t ZMMFullCompNum = 7;
        uint32_t KregsCompNum = 5;
        bool XsaveFeaturesSupported;

        
        void init();
        XsaveComponent getComponentInfo(uint32_t compNum);

        /*
            Assuming Intel PIN does not mess with control registers, we retrieve
            the content of register XCR0 through execution of XGETBV instruction.
            XCR0 is required in order to check which state components are set to be 
            stored by XSAVE
        */
        uint32_t getXcr0Bitmap();

        // Returns true if X87 state should be stored by XSAVE
        bool isX87Stored(uint32_t stateComponentBitmap);

        // Returns true if XMM registers state should be stored by XSAVE
        bool isXmmStored(uint32_t stateComponentBitmap);

        // Returns true if YMM state should be stored by XSAVE
        bool isYmmStored(uint32_t stateComponentBitmap);

        // Returns true if the high bits state of 
        // ZMM registers should be stored by XSAVE
        bool isZmmHighStored(uint32_t stateComponentBitmap);

        // Returns true if additional ZMM registers state should be stored by XSAVE
        bool isZmmFullStored(uint32_t stateComponentBitmap);

        // Returns true if K mask registers state should be stored by XSAVE
        bool isKmaskStored(uint32_t stateComponentBitmap);

    public:
        XsaveHandler(const XsaveHandler& other) = delete;
        void operator=(const XsaveHandler& other) = delete;

        static XsaveHandler& getInstance();

        set<AnalysisArgs> getXsaveAnalysisArgs(CONTEXT* ctxt, OPCODE opcode, ADDRINT addr, UINT32 size);
        set<AnalysisArgs> getXrstorAnalysisArgs(CONTEXT* ctxt, OPCODE opcode, ADDRINT addr, UINT32 size);
};

#endif //XSAVEHANDLER