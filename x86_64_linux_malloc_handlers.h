#include <set>

#include "pin.H"
#include "Platform.h"

size_t malloc_get_block_size(ADDRINT block_addr){
    size_t ret;
    PIN_SafeCopy(&ret, (void*) (block_addr + 8), sizeof(size_t));
    // Heap blocks have always a size multiple of 8. The 3 LSBs are used as boolean flags, so ignore them and 
    // set them to 0.
    ret &= ~ 7;
    return ret;
}

ADDRINT malloc_get_main_heap_upper_bound(ADDRINT block_addr, size_t blockSize){
    ADDRINT top_chunk = block_addr + blockSize;
    // ret is set to the start address of the page immediately following the page
    // the top_chunk belongs to. Therefore the return value will be the last address of the memory page the 
    // top_chunk belongs to
    ADDRINT ret = (top_chunk + PAGE_SIZE) & ~ (PAGE_SIZE - 1);
    --ret;
    return ret;
}

ADDRINT malloc_get_block_beginning(ADDRINT addr){
    return addr - 16;
}

// This function returns a set of pairs representing the portions of an allocated block to be reinitialized on free.
// This is required because in some cases it may lead to false positives reinitializing the whole block
// (e.g. malloc->free->malloc->free, where the second malloc reuses the first freed one).
// On x86_64 architectures, using the glibc implementation of malloc and free, we should also avoid re-initializing
// the first 16 bytes of the payload, as they are written by a call to free in order to keep metadata
// about the freed chunk and may be used by successive calls to malloc.
std::set<std::pair<ADDRINT, size_t>> malloc_mem_to_reinit(ADDRINT block_addr, size_t blockSize){
    std::set<std::pair<ADDRINT, size_t>> ret;
    ret.insert(std::pair<ADDRINT, size_t>(block_addr + 32, blockSize - 32));
    return ret;
}