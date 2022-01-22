#ifndef MALLOC_HANDLER
#define MALLOC_HANDLER

#if defined(CUSTOM_ALLOCATOR)
    // If user compiled to use a custom allocator (or any other malloc functions not contained in the glibc)
    // it is its own responsibility to define the specific handlers for that
    
    // #include "custom_allocator_malloc_handlers.h"
#elif defined(X86_64)
    #if defined(LINUX)
        #include "x86_64_linux_malloc_handlers.h"
    #elif defined(WINDOWS)
        // NOT IMPLEMENTED YET
        // #include "x86_64_windows_malloc_handlers.h"
    #elif defined(MACOS)
        // NOT IMPLEMENTED YET
        // #include "x86_64_macos_malloc_handlers.h"
    #else
        #error "OS not supported yet"
    #endif
#elif defined(X86)
    #if defined(LINUX)
        // NOT IMPLEMENTED YET
        //#include "x86_linux_malloc_handlers.h"
    #elif defined(WINDOWS)
        // NOT IMPLEMENTED YET
        //#include "x86_windows_malloc_handlers.h"
    #elif defined(MACOS)
        // NOT IMPLEMENTED YET
        //#include "x86_macos_malloc_handlers.h"
    #else
        #error "OS not supported yet"
    #endif
#endif

#endif // MALLOC_HANDLER
