#ifndef PLATFORM
#define PLATFORM

#include <list>
#include <unistd.h>
#include "pin.H"

using std::list;

#undef PAGE_SIZE
#define PAGE_SIZE pagesize

static const unsigned long pagesize = sysconf(_SC_PAGESIZE);

// NOTE: this has been tested to work with gcc/g++ compiler. It could not work if another compiler is
// used (e.g. MSVC++, Clang...). If you are using a different compiler, be sure to check and 
// extend this header if needed.

// Architecture detection. Note that Intel PIN only supports X86 and X86_64 architectures. It is useless 
// to try and identify unsupported architectures.
#if defined(__amd64__) || defined(_M_AMD64) || defined(_M_X64)
    #define X86_64
    #define MMAP_NUM 9
    #define BRK_NUM 12
    #define STACK_SHADOW_INIT 0xff
    const REG syscall_args[] = {REG_RDI, REG_RSI, REG_RDX, REG_R10, REG_R8, REG_R9};
#elif defined(__i386__) || defined(_M_IX86)
    #define X86
    #define MMAP_NUM 90
    #define BRK_NUM 45
    #define STACK_SHADOW_INIT 0xf0
    const REG syscall_args[] = {REG_EBX, REG_ECX, REG_EDX, REG_ESI, REG_EDI, REG_EBP};
#else
    #error "Not supported architecture"
#endif

#endif //PLATFORM


// OS detection. The list is not complete. The tool has been developed and tested on a Linux platform.
// If needed, extend this to detect your underlying OS.
// Note that it must support Intel PIN to use this tool.
#if defined(__linux__)
    #define LINUX
    #define STACK_REDZONE_SIZE 128
#elif defined(_WIN32)
    #define WINDOWS
    #define STACK_REDZONE_SIZE 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define MACOS
    #define STACK_REDZONE_SIZE 224
#endif


