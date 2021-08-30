#include <vector>
#include <unordered_map>
#include <string.h>

// Headers required to have definitions of some required structs
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utime.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <ustat.h>
#include <sys/statfs.h>
#include <signal.h>

#include "SyscallMemAccess.h"

#define RETTYPE static set<SyscallMemAccess>
#define ARGUMENTS (ADDRINT retVal, vector<ADDRINT> args)

using std::vector;
using std::unordered_map;
using std::pair;


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
    if((long long) retVal < 0){
        return ret;
    }
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_write_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
    SyscallMemAccess ma(args[1], retVal, AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_pread_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_pwrite_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
    SyscallMemAccess ma(args[1], retVal, AccessType::READ);
    ret.insert(ma);
    return ret;
}

// NOTE: THE NEXT 2 SYSCALLS ARE USED TO HANDLE OTHER SYSCALLS, AS THEY HAVE THE SAME EFFECT.
// CHANGES TO THEM WILL AFFECT THE HANDLING OF THOSE SYSTEM CALLS AS WELL.
RETTYPE sys_readv_handler ARGUMENTS{
    set<SyscallMemAccess> ret;

    if((long long) retVal < 0){
        return ret;
    }

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

    if((long long) retVal < 0){
        return ret;
    }

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
    if((long long) retVal < 0){
        return ret;
    }
    const char* pathname = (char*) args[0];
    SyscallMemAccess pathMA(args[0], strlen(pathname), AccessType::READ);
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_readlinkat_handler ARGUMENTS{
    vector<ADDRINT> readlink_args;
    readlink_args.push_back(args[1]);
    readlink_args.push_back(args[2]);
    readlink_args.push_back(args[3]);
    return sys_readlink_handler(retVal, readlink_args);
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
    if((long long) retVal < 0){
        return ret;
    }
    const char* pathname = (char*) args[0];

    SyscallMemAccess pathMA(args[0], strlen(pathname), AccessType::READ);
    SyscallMemAccess ma(args[1], sizeof(struct stat), AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_fstat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
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


// NOTE: THIS SYSCALL HAS BEEN USED TO HANDLE OTHER SIMILAR SYSCALLS.
// ANY MODIFICATION TO THIS HANDLER WILL AFFECT THOSE SYSCALLS AS WELL
RETTYPE sys_open_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
    const char* pathname = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(pathname), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_creat_handler ARGUMENTS{
    // As 'open', this syscall simply reads the pathname from args[0].
    // Delegate to sys_open_handler.
    return sys_open_handler(retVal, args);
}

RETTYPE sys_openat_handler ARGUMENTS{
    // Works as 'open'. Adjust args vector and delegate to sys_open_handler.
    vector<ADDRINT> open_args;
    open_args.push_back(args[1]);
    open_args.push_back(args[2]);
    open_args.push_back(args[3]);
    return sys_open_handler(retVal, open_args);
}

// NOTE: THIS SYSCALL HANDLER HAS BEEN USED TO HANDLE OTHER SIMILAR SYSCALLS.
// ANY MODIFICATION TO THIS HANDLER WILL AFFECT THOSE SYSCALLS AS WELL.
RETTYPE sys_access_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    const char* pathname = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(pathname), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_faccessat_handler ARGUMENTS{
    // Works as 'access'. Adjust args vector and delegate to sys_access_handler.
    vector<ADDRINT> access_args;
    access_args.push_back(args[1]);
    access_args.push_back(args[2]);
    return sys_access_handler(retVal, access_args);
}

RETTYPE sys_pipe_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0){
        return ret;
    }
    SyscallMemAccess ma(args[0], 2 * sizeof(int), AccessType::WRITE);;
    ret.insert(ma);
    return ret;
}

RETTYPE sys_pipe2_handler ARGUMENTS{
    // Works as 'pipe'. Delegate to sys_pipe_handler.
    return sys_pipe_handler(retVal, args);
}

RETTYPE sys_connect_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], args[2], AccessType::READ);
    ret.insert(ma);
    return ret;
}

// NOTE: THIS HANDLER IS USED TO HANDLE OTHER SYSCALLS.
// ANY MODIFICATION TO THIS HANDLER WILL AFFECT THOSE SYSCALLS AS WELL
RETTYPE sys_accept_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    socklen_t* addrLen = (socklen_t*) args[2];
    SyscallMemAccess ma(args[1], *addrLen, AccessType::WRITE);
    SyscallMemAccess addrLenMA(args[2], sizeof(socklen_t), AccessType::WRITE);
    ret.insert(ma);
    ret.insert(addrLenMA);
    return ret;
}

RETTYPE sys_accept4_handler ARGUMENTS{
    // Works as accept. Delegate to sys_accept_handler.
    return sys_accept_handler(retVal, args);
}

RETTYPE sys_recvfrom_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess bufMA(args[1], retVal, AccessType::WRITE);
    ret.insert(bufMA);
    struct sockaddr* src_addr = (struct sockaddr*)args[4];
    if(src_addr != NULL){
        socklen_t* addrLen = (socklen_t*) args[5];
        SyscallMemAccess addrMA(args[4], MIN(sizeof(struct sockaddr), *addrLen), AccessType::WRITE);
        SyscallMemAccess addrLenMA(args[5], sizeof(socklen_t), AccessType::WRITE);
        ret.insert(addrMA);
        ret.insert(addrLenMA);
    }
    return ret;
}

RETTYPE sys_recvmsg_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    struct msghdr* msg = (struct msghdr*) args[1];
    SyscallMemAccess msghdrMA(args[1], sizeof(struct msghdr), AccessType::READ);
    struct iovec* msg_vec = (struct iovec*) msg->msg_iov;
    size_t msg_iovlen = (size_t) msg->msg_iovlen;

    // This syscalls behaves exactly as readv for msg_vec. Adjust args vector and retrieve written buffers from sys_readv_handler.
    // Note that the first argument of readv is a fd. However that's not useful to register
    // memory operations. Just set it to 0.
    vector<ADDRINT> readv_args;
    readv_args.push_back(0);
    readv_args.push_back((ADDRINT) msg_vec);
    readv_args.push_back((ADDRINT) msg_iovlen);
    set<SyscallMemAccess> readvResult = sys_readv_handler(retVal, readv_args);

    ret.insert(msghdrMA);
    ret.insert(readvResult.begin(), readvResult.end());
    return ret;
}

RETTYPE sys_recvmmsg_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;

    unsigned int vlen = (unsigned int) args[2];
    struct mmsghdr* msgvec = (struct mmsghdr*) args[1];
    for(unsigned int i = 0; i < vlen; ++i, ++msgvec){
        SyscallMemAccess arrMA((ADDRINT)msgvec, sizeof(struct mmsghdr), AccessType::READ);
        // For each msg header, get the corresponding memory operations from sys_recvmsg_handler.
        vector<ADDRINT> recvmsg_args;
        recvmsg_args.push_back(args[0]);
        recvmsg_args.push_back((ADDRINT) msgvec);
        recvmsg_args.push_back(args[3]);
        set<SyscallMemAccess> recvmsgResult = sys_recvmsg_handler(msgvec->msg_len, recvmsg_args);
        ret.insert(recvmsgResult.begin(), recvmsgResult.end());
        ret.insert(arrMA);
    }
    return ret;
}

RETTYPE sys_sendto_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess bufMA(args[1], retVal, AccessType::READ);
    ret.insert(bufMA);
    const struct sockaddr* dest_addr = (struct sockaddr*) args[4];
    if(dest_addr != NULL){
        SyscallMemAccess addrMA(args[4], args[5], AccessType::READ);
        ret.insert(addrMA);
    }
    return ret;
}

RETTYPE sys_sendmsg_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const struct msghdr* msg = (struct msghdr*) args[1];
    SyscallMemAccess msgMA(args[1], sizeof(struct msghdr), AccessType::READ);
    ret.insert(msgMA);
    struct iovec* msg_vec = (struct iovec*) msg->msg_iov;
    size_t msg_iovlen = (size_t) msg->msg_iovlen;

    // This syscalls behaves exactly as writev for msg_vec. Adjust args vector and retrieve read buffers from sys_readv_handler.
    // Note that the first argument of readv is a fd. However that's not useful to register
    // memory operations. Just set it to 0.
    vector<ADDRINT> writev_args;
    writev_args.push_back(0);
    writev_args.push_back((ADDRINT) msg_vec);
    writev_args.push_back((ADDRINT) msg_iovlen);
    set<SyscallMemAccess> writevResult = sys_writev_handler(retVal, writev_args);
    ret.insert(writevResult.begin(), writevResult.end());
    return ret;
}

RETTYPE sys_sendmmsg_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    unsigned int vlen = (unsigned int) args[2];
    struct mmsghdr* arr = (struct mmsghdr*) args[1];
    for(unsigned int i = 0; i < vlen; ++i, ++arr){
        SyscallMemAccess arrMA((ADDRINT) arr, sizeof(struct mmsghdr), AccessType::READ);
        vector<ADDRINT> sendmsg_args;
        sendmsg_args.push_back(args[0]);
        sendmsg_args.push_back((ADDRINT) arr);
        sendmsg_args.push_back(args[3]);
        set<SyscallMemAccess> sendmsg_result = sys_sendmsg_handler(arr->msg_len, sendmsg_args);
        ret.insert(sendmsg_result.begin(), sendmsg_result.end());
        ret.insert(arrMA);
    }
    return ret;
}

RETTYPE sys_bind_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], args[2], AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_getsockname_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess lenMA(args[2], sizeof(socklen_t), AccessType::WRITE);
    socklen_t* addrlen = (socklen_t*) args[2];
    SyscallMemAccess addrMA(args[1], MIN(*addrlen, sizeof(struct sockaddr)), AccessType::WRITE);
    ret.insert(lenMA);
    ret.insert(addrMA);
    return ret;
}

RETTYPE sys_getpeername_handler ARGUMENTS{
    // This syscall has the very same behaviour of getsockname. Just delegate to sys_getsockname_handler.
    return sys_getsockname_handler(retVal, args);
}

RETTYPE sys_socketpair_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[3], 2 * sizeof(int), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_setsockopt_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[3], args[4], AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_getsockopt_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    socklen_t* optlen = (socklen_t*) args[4];
    SyscallMemAccess ma(args[3], *optlen, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_wait4_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal == -1)
        return ret;

    int* wstatus = (int*) args[1];
    struct rusage* rusage = (struct rusage*) args[3];

    if(wstatus != NULL){
        SyscallMemAccess statusMA(args[1], sizeof(int), AccessType::WRITE);
        ret.insert(statusMA);
    }

    if(rusage != NULL){
        SyscallMemAccess rusageMA(args[3], sizeof(struct rusage), AccessType::WRITE);
        ret.insert(rusageMA);
    }

    return ret;
}

RETTYPE sys_uname_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[0], sizeof(struct utsname), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

// This is a quite complex system call. Indeed, its semantic is different according to the value of the |cmd|
// argument. Only in some cases it reads or writes from memory, and some of the commands are not always available
// (from Linux manual, some of them are only available since a specific version of Linux kernel. However, it seems that
// by using Intel PIN, it is not always true. This might depend from the fact that many of the C standard library headers
// have been replaced by Intel PIN itself. In order to avoid compilation errors, those commands with dependencies 
// from the kernel version have been guarded by pre-processor conditionals to check the corresponding value is
// actually defined somewhere)
RETTYPE sys_fcntl_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;

    unsigned int cmd = (unsigned int) args[1];
    switch(cmd){
        case F_SETLK:
        case F_SETLKW:
    #ifdef F_OFD_SETLK
        case F_OFD_SETLK:
    #endif
    #ifdef F_OFD_SETLKW
        case F_OFD_SETLKW:
    #endif
            {
                SyscallMemAccess ma(args[2], sizeof(struct flock), AccessType::READ);
                ret.insert(ma);
                break;
            }
        case F_GETLK:    
    #ifdef F_OFD_GETLK
        case F_OFD_GETLK:
    #endif
            {
                // In this case, the syscall reads info about the lock the user is going to acquire.
                // If it can be acquired, it actually write only field |l_type| of the flock structure,
                // but if that is already locked, the whole structure is overwritten to contain the infos
                // about 1 of the processes holding the lock. For this reason, here we store both a read and a write 
                // access on the same memory area. Note that according to operator< of SyscallMemAccess objects,
                // if 2 accesses have the same address and size, read accesses are considered less than write accesses.
                struct flock* lock = (struct flock*) args[2];
                SyscallMemAccess readMA(args[2], sizeof(struct flock), AccessType::READ);
                ret.insert(readMA);

                if(lock->l_type == F_UNLCK){
                    SyscallMemAccess writeMA((ADDRINT) &(lock->l_type), sizeof(lock->l_type), AccessType::WRITE);
                    ret.insert(writeMA);
                }
                else{
                    SyscallMemAccess writeMA(args[2], sizeof(struct flock), AccessType::WRITE);
                    ret.insert(writeMA);
                }
                break;
            }
        case F_GETOWN_EX:
            {
                SyscallMemAccess ma(args[2], sizeof(struct f_owner_ex), AccessType::WRITE);
                ret.insert(ma);
                break;
            }
        case F_SETOWN_EX:
            {
                SyscallMemAccess ma(args[2], sizeof(struct f_owner_ex), AccessType::READ);
                ret.insert(ma);
                break;
            }
    #if defined(F_GET_RW_HINT) && defined(F_GET_FILE_RW_HINT)
        case F_GET_RW_HINT:
        case F_GET_FILE_RW_HINT:
            {
                SyscallMemAccess ma(args[2], sizeof(uint64_t), AccessType::WRITE);
                ret.insert(ma);
                break;
            }
    #endif
    #if defined(F_SET_RW_HINT) && defined(F_SET_FILE_RW_HINT)
        case F_SET_RW_HINT:
        case F_SET_FILE_RW_HINT:
            {
                SyscallMemAccess ma(args[2], sizeof(uint64_t), AccessType:: READ);
                ret.insert(ma);
                break;
            }
    #endif
        default:
            // Empty default case. Nothing is either read or written in memory
            {}
    }

    return ret;
}

RETTYPE sys_truncate_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_getdents_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;
    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_getcwd_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((char*) retVal == NULL)
        return ret;
    char* buf = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(buf), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_chdir_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_rename_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* old = (char*) args[0];
    char* newpath = (char*) args[1];
    SyscallMemAccess oldMA(args[0], strlen(old), AccessType::READ);
    SyscallMemAccess newMA(args[1], strlen(newpath), AccessType::READ);
    ret.insert(oldMA);
    ret.insert(newMA);
    return ret;
}

RETTYPE sys_renameat_handler ARGUMENTS{
    vector<ADDRINT> rename_args;
    rename_args.push_back(args[1]);
    rename_args.push_back(args[3]);
    return sys_rename_handler(retVal, rename_args);
}

RETTYPE sys_mkdir_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_mkdirat_handler ARGUMENTS{
    vector<ADDRINT> mkdir_args;
    mkdir_args.push_back(args[1]);
    mkdir_args.push_back(args[2]);
    return sys_mkdir_handler(retVal, mkdir_args);
}

RETTYPE sys_rmdir_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_link_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* oldpath = (char*) args[0];
    char* newpath = (char*) args[1];
    SyscallMemAccess oldMA(args[0], strlen(oldpath), AccessType::READ);
    SyscallMemAccess newMA(args[1], strlen(newpath), AccessType::READ);
    ret.insert(oldMA);
    ret.insert(newMA);
    return ret;
}

RETTYPE sys_linkat_handler ARGUMENTS{
    vector<ADDRINT> link_args;
    link_args.push_back(args[1]);
    link_args.push_back(args[3]);
    return sys_link_handler(retVal, link_args);
}

RETTYPE sys_unlink_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_unlinkat_handler ARGUMENTS{
    vector<ADDRINT> unlink_args;
    unlink_args.push_back(args[1]);
    return sys_unlink_handler(retVal, unlink_args);
}

RETTYPE sys_symlink_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* target = (char*) args[0];
    const char* linkpath = (char*) args[1];
    SyscallMemAccess targetMA(args[0], strlen(target), AccessType::READ);
    SyscallMemAccess pathMA(args[1], strlen(linkpath), AccessType::READ);
    ret.insert(targetMA);
    ret.insert(pathMA);
    return ret;
}

RETTYPE sys_symlinkat_handler ARGUMENTS{
    vector<ADDRINT> symlink_args;
    symlink_args.push_back(args[0]);
    symlink_args.push_back(args[2]);
    return sys_symlink_handler(retVal, symlink_args);
}

RETTYPE sys_chmod_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0) 
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_fchmodat_handler ARGUMENTS{
    vector<ADDRINT> chmod_args;
    chmod_args.push_back(args[1]);
    chmod_args.push_back(args[2]);
    return sys_chmod_handler(retVal, chmod_args);
}

RETTYPE sys_chown_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_lchown_handler ARGUMENTS{
    return sys_chown_handler(retVal, args);
}

RETTYPE sys_fchownat_handler ARGUMENTS{
    vector<ADDRINT> chown_args;
    chown_args.push_back(args[1]);
    chown_args.push_back(args[2]);
    chown_args.push_back(args[3]);
    return sys_chown_handler(retVal, chown_args);
}

RETTYPE sys_gettimeofday_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    struct timeval* tv = (struct timeval*) args[0];
    struct timezone* tz = (struct timezone*) args[1];
    if(tv != NULL){
        SyscallMemAccess tvMA(args[0], sizeof(struct timeval), AccessType::WRITE);
        ret.insert(tvMA);
    }
    if(tz != NULL){
        SyscallMemAccess tzMA(args[1], sizeof(struct timezone), AccessType::WRITE);
        ret.insert(tzMA);
    }
    return ret;
}

RETTYPE sys_settimeofday_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const struct timeval* tv = (struct timeval*) args[0];
    const struct timezone* tz = (struct timezone*) args[1];
    if(tv != NULL){
        SyscallMemAccess tvMA(args[0], sizeof(struct timeval), AccessType::READ);
        ret.insert(tvMA);
    }
    if(tz != NULL){
        SyscallMemAccess tzMA(args[1], sizeof(struct timezone), AccessType::READ);
        ret.insert(tzMA);
    }
    return ret;
}

RETTYPE sys_getrlimit_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], sizeof(struct rlimit), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_setrlimit_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], sizeof(struct rlimit), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_prlimit_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    vector<ADDRINT> getrlimit_args;
    vector<ADDRINT> setrlimit_args;
    set<SyscallMemAccess> partialRes;

    struct rlimit* old_limit = (struct rlimit*) args[3];
    if(old_limit != NULL){
        getrlimit_args.push_back(args[1]);
        getrlimit_args.push_back(args[3]);
        partialRes = sys_getrlimit_handler(retVal, getrlimit_args);
        ret.insert(partialRes.begin(), partialRes.end());
        partialRes.clear();
    }

    const struct rlimit* new_limit = (struct rlimit*) args[2];
    if(new_limit != NULL){
        setrlimit_args.push_back(args[1]);
        setrlimit_args.push_back(args[2]);
        partialRes = sys_setrlimit_handler(retVal, setrlimit_args);
        ret.insert(partialRes.begin(), partialRes.end());
    }
    return ret;
}

RETTYPE sys_getrusage_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], sizeof(struct rusage), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_sysinfo_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[0], sizeof(struct sysinfo), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_times_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    // Man page tells the returned value may overflow the possible range, and on error -1 is returned.
    // So, instead of a < comparison, perform an == comparison
    if((time_t) retVal == -1)
        return ret;
    SyscallMemAccess ma(args[0], sizeof(struct tms), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_getgroups_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    gid_t* list = (gid_t*) args[1];
    for(unsigned int i = 0; i < retVal; ++i, ++list){
        SyscallMemAccess ma((ADDRINT) list, sizeof(gid_t), AccessType::WRITE);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_setgroups_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    gid_t* list = (gid_t*) args[1];
    for(unsigned int i = 0; i < args[0]; ++i, ++list){
        SyscallMemAccess ma((ADDRINT) list, sizeof(gid_t), AccessType::READ);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_getresuid_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    size_t size = sizeof(uid_t);
    SyscallMemAccess realMA(args[0], size, AccessType::WRITE);
    SyscallMemAccess effectiveMA(args[1], size, AccessType::WRITE);
    SyscallMemAccess setMA(args[2], size, AccessType::WRITE);
    ret.insert(realMA);
    ret.insert(effectiveMA);
    ret.insert(setMA);
    return ret;
}

RETTYPE sys_getresgid_handler ARGUMENTS{
    return sys_getresuid_handler(retVal, args);
}

RETTYPE sys_utime_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* filename = (char*) args[0];
    SyscallMemAccess filenameMA(args[0], strlen(filename), AccessType::READ);
    ret.insert(filenameMA);
    const struct utimbuf* times = (struct utimbuf*) args[1];
    if(times != NULL){
        SyscallMemAccess timesMA(args[1], sizeof(struct utimbuf), AccessType::READ);
        ret.insert(timesMA);
    }
    return ret;
}

RETTYPE sys_utimes_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* filename = (char*) args[0];
    SyscallMemAccess filenameMA(args[0], strlen(filename), AccessType::READ);
    ret.insert(filenameMA);
    const struct timeval* times = (struct timeval*) args[1];
    if(times != NULL){
        SyscallMemAccess timesMA(args[1], 2 * sizeof(struct timeval), AccessType::READ);
        ret.insert(timesMA);
    }
    return ret;
}

RETTYPE sys_waitid_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal == -1)
        return ret;

    struct siginfo* infop = (struct siginfo*) args[2];
    struct rusage* rusage = (struct rusage*) args[4];

    if(infop != NULL){
        SyscallMemAccess infopMA(args[2], sizeof(*infop), AccessType::WRITE);
        ret.insert(infopMA);
    }

    if(rusage != NULL){
        SyscallMemAccess rusageMA(args[4], sizeof(*rusage), AccessType::WRITE);
        ret.insert(rusageMA);
    }

    return ret;
}

RETTYPE sys_utimensat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* pathname = (char*) args[1];
    SyscallMemAccess pathMA(args[1], strlen(pathname), AccessType::READ);
    const struct timespec* times = (struct timespec*) args[2];
    if(times != NULL){
        SyscallMemAccess timesMA(args[2], 2 * sizeof(struct timespec), AccessType::READ);
        ret.insert(timesMA);
    }
    return ret;
}

// Syscall number not found
RETTYPE sys_futimens_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const struct timespec* times = (struct timespec*) args[1];
    if(times != NULL){
        SyscallMemAccess ma(args[1], 2 * sizeof(struct timespec), AccessType::READ);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_mknod_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_mknodat_handler ARGUMENTS{
    vector<ADDRINT> mknod_args;
    mknod_args.push_back(args[1]);
    mknod_args.push_back(args[2]);
    mknod_args.push_back(args[3]);
    return sys_mknod_handler(retVal, mknod_args);
}

RETTYPE sys_ustat_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], sizeof(struct ustat), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_statfs_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess pathMA(args[0], strlen(path), AccessType::READ);
    SyscallMemAccess bufMA(args[1], sizeof(struct statfs), AccessType::WRITE);
    ret.insert(pathMA);
    ret.insert(bufMA);
    return ret;
}

RETTYPE sys_fstatfs_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    SyscallMemAccess ma(args[1], sizeof(struct statfs), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_chroot_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* path = (char*) args[0];
    SyscallMemAccess ma(args[0], strlen(path), AccessType::READ);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_acct_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* filename = (char*) args[0];
    if(filename != NULL){
        SyscallMemAccess ma(args[0], strlen(filename), AccessType::READ);
        ret.insert(ma);
    }
    return ret;
}

// Syscall number not found
RETTYPE sys_gethostname_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0) 
        return ret;
    char* name = (char*) args[0];
    SyscallMemAccess ma(args[0], MIN(args[1], strlen(name) + 1), AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_sethostname_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    const char* name = (char*) args[0];
    SyscallMemAccess ma(args[0], MIN(args[1], strlen(name)), AccessType::READ);
    ret.insert(ma);
    return ret;
}

// Syscall number not found
RETTYPE sys_getdomainname_handler ARGUMENTS{
    return sys_gethostname_handler(retVal, args);
}

RETTYPE sys_setdomainname_handler ARGUMENTS{
    return sys_sethostname_handler(retVal, args);
}

RETTYPE sys_time_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((time_t) retVal == -1)
        return ret;
    time_t* tloc = (time_t*) args[0];
    if(tloc != NULL){
        SyscallMemAccess ma(args[0], sizeof(time_t), AccessType::WRITE);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_getdents64_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;

    SyscallMemAccess ma(args[1], retVal, AccessType::WRITE);
    ret.insert(ma);
    return ret;
}

RETTYPE sys_clock_settime ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;

    const struct timespec* tp = (struct timespec*) args[1];
    if(tp != NULL){
        SyscallMemAccess ma(args[1], sizeof(struct timespec), AccessType::READ);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_clock_gettime ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;
    
    struct timespec* tp = (struct timespec*) args[1];
    if(tp != NULL){
        SyscallMemAccess ma(args[1], sizeof(struct timespec), AccessType::WRITE);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_clock_getres ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((int) retVal == -1)
        return ret;

    struct timespec* res = (struct timespec*) args[1];
    if(res != NULL){
        SyscallMemAccess ma(args[1], sizeof(struct timespec), AccessType::WRITE);
        ret.insert(ma);
    }
    return ret;
}

RETTYPE sys_clock_nanosleep ARGUMENTS{
    set<SyscallMemAccess> ret;
    // NOTE: if this system call is interrupted, it returns EINTR and writes inside |remain| the
    if(retVal != 0 && retVal != EINTR)
        return ret;

    struct timespec* request = (struct timespec*) args[2];
    struct timespec* remain = (struct timespec*) args[3];

    if(request != NULL){
        SyscallMemAccess ma(args[2], sizeof(struct timespec), AccessType::READ);
        ret.insert(ma);
    }

    // |remain| is used only if the flag passed in args[1] is not TIMER_ABSTIME.
    // NOTE: |remain| is only written if the system call return EINTR.
    // If the system call terminates without interrupts, remain will be untouched, so if the
    // user reads from that, it will read junk, or anything else there was written before calling the
    // system call
    if(remain != NULL && retVal == EINTR && args[1] != TIMER_ABSTIME){
        SyscallMemAccess ma(args[3], sizeof(struct timespec), AccessType::WRITE);
        ret.insert(ma);
    }

    return ret;
}

RETTYPE sys_rt_sigaction_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;
    
    struct sigaction* act = (struct sigaction*) args[1];
    struct sigaction* oldact = (struct sigaction*) args[2];

    if(act != NULL){
        SyscallMemAccess actMA(args[1], sizeof(struct sigaction), AccessType::READ);
        ret.insert(actMA);
    }

    if(oldact != NULL){
        SyscallMemAccess oldactMA(args[2], sizeof(struct sigaction), AccessType::WRITE);
        ret.insert(oldactMA);
    }

    return ret;
}

RETTYPE sys_rt_sigprocmask_handler ARGUMENTS{
    set<SyscallMemAccess> ret;
    if((long long) retVal < 0)
        return ret;

    sigset_t* set = (sigset_t*) args[1];
    sigset_t* oldset = (sigset_t*) args[2];

    if(set != NULL){
        SyscallMemAccess setMA(args[1], sizeof(sigset_t), AccessType::READ);
        ret.insert(setMA);
    }

    if(oldset != NULL){
        SyscallMemAccess oldsetMA(args[2], sizeof(sigset_t), AccessType::WRITE);
        ret.insert(oldsetMA);
    }

    return ret;
}



//////////////////////////////////////////////////////////////////
//                      HANDLER SELECTOR                        //
//  The only thing to do here is adding the new defined         //
//  handler into 'handlers' map, inside method 'init()'         //
//  by using the macro 'SYSCALL_ENTRY'.                         //
//  Macro's signature:                                          //    
//  SYSCALL_ENTRY(<Syscall_Number>, <Args_Number>, <Handler>).  //
//  <Args_Number> is the number of arguments the system call    //
//  requires (e.g. read system call requires 3 arguments).      //
//  If no new handler has been defined, then nothing has to be  //
//  done here.                                                  //
//////////////////////////////////////////////////////////////////

typedef set<SyscallMemAccess>(*HandlerFunction)(ADDRINT, vector<ADDRINT>);

#define SYSCALL_ENTRY(SYS_NUM, SYS_ARGS, SYS_HANDLER) \
handlers.insert(pair<unsigned short, HandlerFunction>((SYS_NUM), (SYS_HANDLER)));\
argsCount.insert(pair<unsigned short, unsigned short>((SYS_NUM), (SYS_ARGS)))

class HandlerSelector{
    private:
        unordered_map<unsigned short, HandlerFunction> handlers;
        unordered_map<unsigned short, unsigned short> argsCount;

        void init(){
            SYSCALL_ENTRY(0, 3, sys_read_handler);
            SYSCALL_ENTRY(1, 3, sys_write_handler);
            SYSCALL_ENTRY(2, 3, sys_open_handler);
            SYSCALL_ENTRY(4, 2, sys_stat_handler);
            SYSCALL_ENTRY(5, 2, sys_fstat_handler);
            SYSCALL_ENTRY(6, 2, sys_lstat_handler);
            SYSCALL_ENTRY(13, 4, sys_rt_sigaction_handler);
            SYSCALL_ENTRY(14, 4, sys_rt_sigprocmask_handler);
            SYSCALL_ENTRY(17, 4, sys_pread_handler);
            SYSCALL_ENTRY(18, 4, sys_pwrite_handler);
            SYSCALL_ENTRY(19, 3, sys_readv_handler);
            SYSCALL_ENTRY(20, 3, sys_writev_handler);
            SYSCALL_ENTRY(21, 2, sys_access_handler);
            SYSCALL_ENTRY(22, 1, sys_pipe_handler);
            SYSCALL_ENTRY(42, 3, sys_connect_handler);
            SYSCALL_ENTRY(43, 3, sys_accept_handler);
            SYSCALL_ENTRY(44, 6, sys_sendto_handler);
            SYSCALL_ENTRY(45, 6, sys_recvfrom_handler);
            SYSCALL_ENTRY(46, 3, sys_sendmsg_handler);
            SYSCALL_ENTRY(47, 3, sys_recvmsg_handler);
            SYSCALL_ENTRY(49, 3, sys_bind_handler);
            SYSCALL_ENTRY(51, 3, sys_getsockname_handler);
            SYSCALL_ENTRY(52, 3, sys_getpeername_handler);
            SYSCALL_ENTRY(53, 4, sys_socketpair_handler);
            SYSCALL_ENTRY(54, 5, sys_setsockopt_handler);
            SYSCALL_ENTRY(55, 5, sys_getsockopt_handler);
            SYSCALL_ENTRY(61, 4, sys_wait4_handler);
            SYSCALL_ENTRY(63, 1, sys_uname_handler);
            SYSCALL_ENTRY(72, 3, sys_fcntl_handler);
            SYSCALL_ENTRY(76, 2, sys_truncate_handler);
            SYSCALL_ENTRY(78, 3, sys_getdents_handler);
            SYSCALL_ENTRY(79, 2, sys_getcwd_handler);
            SYSCALL_ENTRY(80, 1, sys_chdir_handler);
            SYSCALL_ENTRY(82, 2, sys_rename_handler);
            SYSCALL_ENTRY(83, 2, sys_mkdir_handler);
            SYSCALL_ENTRY(84, 1, sys_rmdir_handler);
            SYSCALL_ENTRY(85, 2, sys_creat_handler);
            SYSCALL_ENTRY(86, 2, sys_link_handler);
            SYSCALL_ENTRY(87, 1, sys_unlink_handler);
            SYSCALL_ENTRY(88, 2, sys_symlink_handler);
            SYSCALL_ENTRY(89, 3, sys_readlink_handler);
            SYSCALL_ENTRY(90, 2, sys_chmod_handler);
            SYSCALL_ENTRY(92, 3, sys_chown_handler);
            SYSCALL_ENTRY(94, 3, sys_lchown_handler);
            SYSCALL_ENTRY(96, 2, sys_gettimeofday_handler);
            SYSCALL_ENTRY(97, 2, sys_getrlimit_handler);
            SYSCALL_ENTRY(98, 2, sys_getrusage_handler);
            SYSCALL_ENTRY(99, 1, sys_sysinfo_handler);
            SYSCALL_ENTRY(100, 1, sys_times_handler);
            SYSCALL_ENTRY(115, 2, sys_getgroups_handler);
            SYSCALL_ENTRY(116, 2, sys_setgroups_handler);
            SYSCALL_ENTRY(118, 3, sys_getresuid_handler);
            SYSCALL_ENTRY(120, 3, sys_getresgid_handler);
            SYSCALL_ENTRY(132, 2, sys_utime_handler);
            SYSCALL_ENTRY(133, 3, sys_mknod_handler);
            SYSCALL_ENTRY(136, 2, sys_ustat_handler);
            SYSCALL_ENTRY(137, 2, sys_statfs_handler);
            SYSCALL_ENTRY(138, 2, sys_fstatfs_handler);
            SYSCALL_ENTRY(160, 2, sys_setrlimit_handler);
            SYSCALL_ENTRY(161, 1, sys_chroot_handler);
            SYSCALL_ENTRY(163, 1, sys_acct_handler);
            SYSCALL_ENTRY(164, 2, sys_settimeofday_handler);
            SYSCALL_ENTRY(170, 2, sys_sethostname_handler);
            SYSCALL_ENTRY(171, 2, sys_setdomainname_handler);
            SYSCALL_ENTRY(201, 1, sys_time_handler);
            SYSCALL_ENTRY(217, 3, sys_getdents64_handler);
            SYSCALL_ENTRY(227, 2, sys_clock_settime);
            SYSCALL_ENTRY(228, 2, sys_clock_gettime);
            SYSCALL_ENTRY(229, 2, sys_clock_getres);
            SYSCALL_ENTRY(230, 4, sys_clock_nanosleep);
            SYSCALL_ENTRY(235, 2, sys_utimes_handler);
            SYSCALL_ENTRY(247, 5, sys_waitid_handler);
            SYSCALL_ENTRY(257, 4, sys_openat_handler);
            SYSCALL_ENTRY(258, 3, sys_mkdirat_handler);
            SYSCALL_ENTRY(259, 4, sys_mknodat_handler);
            SYSCALL_ENTRY(260, 5, sys_fchownat_handler);
            SYSCALL_ENTRY(262, 4, sys_fstatat_handler);
            SYSCALL_ENTRY(263, 3, sys_unlinkat_handler);
            SYSCALL_ENTRY(264, 4, sys_renameat_handler);
            SYSCALL_ENTRY(265, 5, sys_linkat_handler);
            SYSCALL_ENTRY(266, 3, sys_symlinkat_handler);
            SYSCALL_ENTRY(267, 4, sys_readlinkat_handler);
            SYSCALL_ENTRY(268, 4, sys_fchmodat_handler);
            SYSCALL_ENTRY(269, 4, sys_faccessat_handler);
            SYSCALL_ENTRY(280, 4, sys_utimensat_handler);
            SYSCALL_ENTRY(288, 4, sys_accept4_handler);
            SYSCALL_ENTRY(293, 2, sys_pipe2_handler);
            SYSCALL_ENTRY(295, 4, sys_preadv_handler);
            SYSCALL_ENTRY(296, 4, sys_pwritev_handler);
            SYSCALL_ENTRY(299, 5, sys_recvmmsg_handler);
            SYSCALL_ENTRY(302, 4, sys_prlimit_handler);
            SYSCALL_ENTRY(307, 4, sys_sendmmsg_handler);
            SYSCALL_ENTRY(310, 6, sys_process_vm_readv_handler);
            SYSCALL_ENTRY(311, 6, sys_process_vm_writev_handler);

            // Insert unused handlers into a vector in order to avoid compilation warnings
            // which are converted into errors by Intel PIN
            vector<HandlerFunction> unused;
            unused.push_back(sys_getdomainname_handler);
            unused.push_back(sys_futimens_handler);
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

        unsigned short getSyscallArgsCount(unsigned short sysNum){
            auto count = argsCount.find(sysNum);
            return count == argsCount.end() ? 0 : count->second;
        }

};