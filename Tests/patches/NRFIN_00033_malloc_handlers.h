#include <set>

#include "pin.H"
#include "Platform.h"

typedef struct heap_chunk heap_chunk_t;

struct heap_chunk {
  heap_chunk_t *prev;
  heap_chunk_t *next;
  uint32_t size;
};

size_t malloc_get_block_size(ADDRINT block_addr){
    size_t ret;
    heap_chunk_t* blk = (heap_chunk_t*) block_addr;
    void* size_ptr = &(blk->size);
    PIN_SafeCopy(&ret, size_ptr, sizeof(size_t));

    return ret;
}

ADDRINT malloc_get_main_heap_upper_bound(ADDRINT block_addr, size_t blockSize){
    ADDRINT page_beginning = block_addr &= (PAGE_SIZE - 1);
    ADDRINT ret = page_beginning + PAGE_SIZE - 1;
    return ret;
}

ADDRINT malloc_get_block_beginning(ADDRINT addr){
    return addr - sizeof(heap_chunk_t);
}

// This function returns a set of pairs representing the portions of an allocated block to be reinitialized on free.
// This is required because in some cases it may lead to false positives reinitializing the whole block
// (e.g. malloc->free->malloc->free, where the second malloc reuses the first freed one).
// On x86_64 architectures, using the glibc implementation of malloc and free, we should also avoid re-initializing
// the first 16 bytes of the payload, as they are written by a call to free in order to keep metadata
// about the freed chunk and may be used by successive calls to malloc.
std::set<std::pair<ADDRINT, size_t>> malloc_mem_to_reinit(ADDRINT block_addr, size_t blockSize){
    std::set<std::pair<ADDRINT, size_t>> ret;
    size_t header_size = sizeof(heap_chunk_t);
    ret.insert(std::pair<ADDRINT, size_t>(block_addr + header_size, blockSize - header_size));
    return ret;
}