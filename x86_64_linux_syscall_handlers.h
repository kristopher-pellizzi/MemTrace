#include <vector>
#include <unordered_map>
#include <string.h>

// Headers required to have definitions of some required structs
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "SyscallMemAccess.h"

#define RETTYPE static set<SyscallMemAccess>
#define ARGUMENTS (ADDRINT retVal, vector<ADDRINT> args)

using std::vector;
using std::unordered_map;

// NOTE: REMEMBER THAT IT IS ALSO NECESSARY TO ADD THE SIGNATURE OF 
// ANY NEW SYSTEM CALL ADDED TO THE HANDLERS.
// SEE <ARCH>_syscall_signatures.yaml FOR MORE DETAILS.

/////////////////////////////////////////////////////////////////
//                      SYSCALL HANDLERS                       //
//  Here syscall handlers are implemented. If you have to      //
//  implement a new handler, add it here, with the same return //
//  type and arguments. After having declared and implemented  //
//  it, just add it to the HandlerSelector class below         //
//  (instructions below).                                      //
//  If you just need to modify an handler, just modify it,     //
//  nothing else is needed.                                    //      
/////////////////////////////////////////////////////////////////


RETTYPE sys_read_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_write_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_pread_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_pwrite_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::READ);
    ret.insert(ma);
    return ret;
}

// NOTE: THE NEXT 2 SYSCALLS ARE USED TO HANDLE OTHER SYSCALLS, AS THEY HAVE THE SAME EFFECT.
// CHANGES TO THEM WILL AFFECT THE HANDLING OF THOSE SYSTEM CALLS AS WELL.
RETTYPE sys_readv_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const struct iovec* bufs = (struct iovec*) args[1];
    int buf_cnt = args[2];
    UINT32 stillToBeReadCnt = retVal;

    for(int i = 0; i < buf_cnt && stillToBeReadCnt > 0; ++i){
        ADDRINT readArrayCell = (ADDRINT) &bufs[i];
        // The syscall reads the address of the buffer from the array of struct iovec...
        SyscallMemAccess vecAccess(readArrayCell, sizeof(struct iovec), AccessType::READ);
        void* bufBase = bufs[i].iov_base;
        UINT32 insertedIntoBufferCnt = MIN(bufs[i].iov_len, stillToBeReadCnt);
        stillToBeReadCnt -= insertedIntoBufferCnt;
        // ... then it writes the read bytes into the buffer, attempting to fill it.
        SyscallMemAccess bufAccess((ADDRINT) bufBase, insertedIntoBufferCnt, AccessType::WRITE);
        ret.insert(vecAccess);
        ret.insert(bufAccess);
    }
    return ret;
}

RETTYPE sys_writev_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const struct iovec* bufs = (struct iovec*) args[1];
    int buf_cnt = args[2];
    UINT32 stillToBeWrittenCnt = retVal;

    for(int i = 0; i < buf_cnt && stillToBeWrittenCnt > 0; ++i){
        ADDRINT readArrayCell = (ADDRINT) &bufs[i];
        // The syscall reads the address of the buffer from the array of struct iovec...
        SyscallMemAccess vecAccess(readArrayCell, sizeof(struct iovec), AccessType::READ);
        void* bufBase = bufs[i].iov_base;
        UINT32 writtenFromBufferCnt = MIN(bufs[i].iov_len, stillToBeWrittenCnt);
        stillToBeWrittenCnt -= writtenFromBufferCnt;
        // ... then it writes the read bytes into the file described from fd.
        SyscallMemAccess bufAccess((ADDRINT) bufBase, writtenFromBufferCnt, AccessType::READ);
        ret.insert(vecAccess);
        ret.insert(bufAccess);
    }
    return ret;
}

RETTYPE sys_readlink_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const char* pathname = (char*) args[0];
    SyscallMemAccess pathMA(args[0], strlen(pathname), AccessType::READ);
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_readlinkat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const char* pathname = (char*) args[1];
    SyscallMemAccess pathMA(args[1], strlen(pathname), AccessType::READ);
    SyscallMemAccess ma(args[2], retVal, AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(ma);
    return ret;
}

// The next 2 syscalls have 1 additional argument w.r.t. readv/writev, but it is added as a 4th argument. Arguments at index 0, 1 and 2
// (those interesting to detect memory read/write) are the same.
RETTYPE sys_preadv_handler ARGUMENTS{
    return sys_readv_handler(retVal, args);
}

RETTYPE sys_pwritev_handler ARGUMENTS{
    return sys_writev_handler(retVal, args);
}

// The next 2 syscalls transfer data between 2 processes address spaces. Since our tool is thought to detect overlaps
// inside 1 process (at least for now), we'll keep trace only of local reads/writes.
// NOTE: THIS HAS TO BE CHANGED IF THE TOOL IS MODIFIED TO MANAGE MULTI-THREADED/MULTI-PROCESS APPLICATIONS
RETTYPE sys_process_vm_readv_handler ARGUMENTS{
    // This syscall has the same effect (locally) of readv. So, just call readv handler after having adjusted
    // syscall arguments to fit readv arguments.
    vector<ADDRINT> readv_args;
    // First argument is the fd of the file to read from. In this case, there's no fd, so just set it to 0
    // In any case, it is not useful to keep track of read/written memory areas.
    readv_args.push_back(0);
    readv_args.push_back((ADDRINT) args[1]);
    readv_args.push_back((ADDRINT) args[2]);
    return sys_readv_handler(retVal, readv_args);
}

RETTYPE sys_process_vm_writev_handler ARGUMENTS{
    // This syscall has the same effect (locally) of readv. So, just call readv handler after having adjusted
    // syscall arguments to fit readv arguments.
    vector<ADDRINT> writev_args;
    // First argument is the fd of the file to read from. In this case, there's no fd, so just set it to 0
    // In any case, it is not useful to keep track of read/written memory areas.
    writev_args.push_back(0);
    writev_args.push_back((ADDRINT) args[1]);
    writev_args.push_back((ADDRINT) args[2]);
    return sys_writev_handler(retVal, writev_args);
}

// NOTE: THIS SYSCALL IS USED AS A HANDLER FOR OTHER SYSCALLS HAVING THE SAME MEMORY BEHAVIOUR.
// ANY MODIFICATION TO THIS HANDLER WILL AFFECT THOSE SYSCALLS AS WELL
RETTYPE sys_stat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const char* pathname = (char*) args[0];

    SyscallMemAccess pathMA(args[0], strlen(pathname), AccessType::READ);
    SyscallMemAccess ma(args[1], sizeof(struct stat), AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_fstat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    SyscallMemAccess ma(args[1], sizeof(struct stat), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_lstat_handler ARGUMENTS{
    return sys_stat_handler(retVal, args);
}

RETTYPE sys_fstatat_handler ARGUMENTS{
    vector<ADDRINT> stat_args;
    stat_args.push_back(args[1]);
    stat_args.push_back(args[2]);
    return sys_stat_handler(retVal, stat_args);
}


//////////////////////////////////////////////////////////////////
//                      HANDLER SELECTOR                        //
//  The only thing to do here is adding the new defined         //
//  handler into 'handlers' map, inside method 'init()'.        //
//  If no new handler has been defined, then nothing has to be  //
//  done here.                                                  //
//////////////////////////////////////////////////////////////////

typedef set<SyscallMemAccess>(*HandlerFunction)(ADDRINT, vector<ADDRINT>);
#define SYSCALL_ENTRY(SYS_NUM, SYS_HANDLER) handlers.insert(pair<unsigned short, HandlerFunction>((SYS_NUM), (SYS_HANDLER)))

class HandlerSelector{
    private:
        unordered_map<unsigned short, HandlerFunction> handlers;

        void init(){
            SYSCALL_ENTRY(0, sys_read_handler);
            SYSCALL_ENTRY(1, sys_write_handler);
            SYSCALL_ENTRY(4, sys_stat_handler);
            SYSCALL_ENTRY(5, sys_fstat_handler);
            SYSCALL_ENTRY(6, sys_lstat_handler);
            SYSCALL_ENTRY(17, sys_pread_handler);
            SYSCALL_ENTRY(18, sys_pwrite_handler);
            SYSCALL_ENTRY(19, sys_readv_handler);
            SYSCALL_ENTRY(20, sys_writev_handler);
            SYSCALL_ENTRY(89, sys_readlink_handler);
            SYSCALL_ENTRY(262, sys_fstatat_handler);
            SYSCALL_ENTRY(267, sys_readlinkat_handler);
            SYSCALL_ENTRY(295, sys_preadv_handler);
            SYSCALL_ENTRY(296, sys_pwritev_handler);
            SYSCALL_ENTRY(310, sys_process_vm_readv_handler);
            SYSCALL_ENTRY(311, sys_process_vm_writev_handler);
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