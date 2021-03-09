#include <iostream>
#include <set>

#ifdef __amd64__
    #define X86_64
    #include "x86_64_linux_syscall_handlers.h"
#elif defined __i386__
    #define X86
#else
    #error "Not supported architecture"
#endif

using std::endl;
using std::set;

// State pattern for the syscall handler

class SyscallHandler{

    class SyscallHandlerUnsetState;
    class SyscallHandlerSysEntryState;
    class SyscallHandlerSysExitState;

    class SyscallHandlerState{

        protected:
            unsigned short sysNum;
            ADDRINT retVal;
            // args actually contain only syscall arguments which are stack addresses
            vector<ADDRINT> args;
            ADDRINT lastSp;
            ADDRINT lastBp;

            void stateError(const char* msg){
                std::cerr << msg << " (" << this->getName() << ")" << endl;
                exit(1);
            }

        public:
            virtual SyscallHandlerState* setSysArgs(unsigned short sysNum, vector<ADDRINT>& args){
                stateError("Setting system call arguments is not valid at this state");
                return NULL;
            }

            virtual SyscallHandlerState* setSysRet(ADDRINT sysRet){
                stateError("Setting system call return value is not valid at this state");
                return NULL;
            }

            virtual set<SyscallMemAccess> getReadsWrites(){
                stateError("It's not possible to retrieve syscall readings and writings at this state");
                return set<SyscallMemAccess>();
            }

            virtual const std::string getName(){
                return std::string("Generic State");
            }

            // Method used to pass a reference and avoid copying a vector on state transition
            vector<ADDRINT>& getArgs(){
                return args;
            }

            virtual ~SyscallHandlerState(){};

            friend class SyscallHandlerUnsetState;
            friend class SyscallHandlerSysEntryState;
            friend class SyscallHandlerSysExitState;
    };





    class SyscallHandlerSysExitState : public SyscallHandlerState{
        private: 
            SyscallHandlerState* nextState(){
                return this;
            }

        public:
            SyscallHandlerSysExitState(SyscallHandlerState* entryState){
                this->sysNum = entryState->sysNum;
                this->retVal = entryState->retVal;
                this->args = entryState->getArgs();
            }

            SyscallHandlerSysExitState(const SyscallHandlerSysExitState& other) = delete;
            SyscallHandlerSysExitState& operator=(const SyscallHandlerSysExitState& other) = delete;

            const std::string getName() override{
                return std::string("Sys Exit Name");
            }
            
            set<SyscallMemAccess> getReadsWrites() override{
                return  HandlerSelector::getInstance().handle_syscall(
                            this->sysNum, 
                            this->retVal,
                            this->args
                        );
            }
    };





    class SyscallHandlerSysEntryState : public SyscallHandlerState{
        private:
            SyscallHandlerState* nextState(){
                SyscallHandlerSysExitState* state = new SyscallHandlerSysExitState(this);
                return state;
            }

        public:
            SyscallHandlerSysEntryState(SyscallHandlerState* unsetState){
                this->sysNum = unsetState->sysNum;
                this->args = unsetState->getArgs();
            }

            SyscallHandlerSysEntryState(const SyscallHandlerSysEntryState& other) = delete;
            SyscallHandlerSysEntryState& operator=(const SyscallHandlerSysEntryState& other) = delete;

            const std::string getName() override{
                return std::string("Sys Entry Name");
            }
            
            SyscallHandlerState* setSysRet(ADDRINT sysRet) override{
                this->retVal = sysRet;
                SyscallHandlerState* state = nextState();
                delete this;
                return state;
            }
    };




    class SyscallHandlerUnsetState : public SyscallHandlerState{
        private:
            SyscallHandlerState* nextState(){
                SyscallHandlerSysEntryState* state = new SyscallHandlerSysEntryState(this);
                return state;
            }

        public:
            SyscallHandlerUnsetState(){}

            SyscallHandlerUnsetState(const SyscallHandlerUnsetState& other) = delete;
            SyscallHandlerUnsetState& operator=(const SyscallHandlerUnsetState& other) = delete;

            const std::string getName() override{
                return std::string("Unset State");
            }
            
            SyscallHandlerState* setSysArgs(unsigned short sysNum, vector<ADDRINT>& args) override{
                this->sysNum = sysNum;
                this->args = vector<ADDRINT>(args);
                SyscallHandlerState* state = nextState();
                delete this;
                return state;
            }

    };


    private:
        SyscallHandlerState* currentState;

        void initHandler(){
            currentState = new SyscallHandlerUnsetState();
        }                    

        SyscallHandler(){
            initHandler();
        }

    public:

        SyscallHandler(const SyscallHandler& other) = delete;
        SyscallHandler& operator=(const SyscallHandler& other) = delete;

        static SyscallHandler& getInstance(){
            static SyscallHandler instance;
            return instance;
        }

        void setSysArgs(unsigned short sysNum, vector<ADDRINT> actualArgs){
            currentState = currentState->setSysArgs(sysNum, actualArgs);
        }

        void setSysRet(ADDRINT retVal){
            currentState = currentState->setSysRet(retVal);
        }

        set<SyscallMemAccess> getReadsWrites(){
            set<SyscallMemAccess> ret = currentState->getReadsWrites();
            initHandler();
            return ret;
        }

        std::string getStateName(){
            return currentState->getName();
        }

        unsigned short getSyscallArgsCount(unsigned short sysNum){
            return HandlerSelector::getInstance().getSyscallArgsCount(sysNum);
        }
};