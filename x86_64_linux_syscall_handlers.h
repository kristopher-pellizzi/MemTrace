#include <vector>
#include <unordered_map>

#include "SyscallMemAccess.h"

using std::vector;
using std::unordered_map;

static set<SyscallMemAccess> sys_read_handler(ADDRINT retVal, vector<ADDRINT> args){
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

static set<SyscallMemAccess> sys_write_handler(ADDRINT retVal, vector<ADDRINT> args){
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::READ);
    ret.insert(ma);
    return ret;
}

typedef set<SyscallMemAccess>(*HandlerFunction)(ADDRINT, vector<ADDRINT>);

class HandlerSelector{
    private:
        unordered_map<unsigned short, HandlerFunction> handlers;

        void init(){
            handlers.insert(pair<unsigned short, HandlerFunction>(0, sys_read_handler));
            handlers.insert(pair<unsigned short, HandlerFunction>(1, sys_write_handler));

        }

        HandlerSelector(){
            init();
        };

    public:
        HandlerSelector(const HandlerSelector& other) = delete;
        HandlerSelector& operator=(const HandlerSelector& other) = delete;

        static HandlerSelector& getInstance(){
            static HandlerSelector instance;
            return instance;
        }

        set<SyscallMemAccess> handle_syscall(unsigned short sysNum, ADDRINT retVal, vector<ADDRINT> args){
            auto handler = handlers.find(sysNum);
            // If there's an handler in the map, execute it and return the result;
            // otherwise, just return an empty set
            return handler == handlers.end() ? set<SyscallMemAccess>() : set<SyscallMemAccess>((*handler->second)(retVal, args));
        }

};