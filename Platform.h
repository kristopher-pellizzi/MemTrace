#ifndef PLATFORM
#define PLATFORM

#include <unistd.h>

#undef PAGE_SIZE
#define PAGE_SIZE pagesize

static const unsigned long pagesize = sysconf(_SC_PAGESIZE);

// NOTE: this has been tested to work with gcc/g++ compiler. It could not work if another compiler is
// used (e.g. MSVC++, Clang...). If you are using a different compiler, be sure to extend this header

// Architecture detection. Note that Intel PIN only supports X86 and X86_64 architectures. It is useless 
// to try and identify unsupported architectures.
#if defined(__amd64__)
    #define X86_64
    #define MMAP_NUM 9
    #define STACK_SHADOW_INIT 0xff
#elif defined(__i386__)
    #define X86
    #define MMAP_NUM 90
    #define STACK_SHADOW_INIT 0xf0
#else
    #error "Not supported architecture"
#endif

#endif //PLATFORM


// OS detection. The list is not complete. The tool has been developed and tested on a Linux platform.
// If needed, extend this to detect your underlying OS
#if defined(__linux__)
    #define LINUX
#elif defined(_WIN32)
    #define WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
    #define MACOS
#endif


