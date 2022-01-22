#include <set>

#include "pin.H"
#include "Platform.h"

// These are from the custom file malloc.h
#define NUM_FREE_LISTS 32
#define HEADER_PADDING sizeof(struct blk_t)
#define NEW_CHUNK_SIZE 262144
#define ALIGNMENT 8

struct blk_t {
  size_t size;
  unsigned int free;
  struct blk_t *fsucc;
  struct blk_t *fpred;
  struct blk_t *next;
  struct blk_t *prev;
};

size_t malloc_get_block_size(ADDRINT block_addr){
    size_t ret;
    struct blk_t* blk = (struct blk_t*) block_addr;
    void* size_ptr = &(blk->size);
    PIN_SafeCopy(&ret, size_ptr, sizeof(size_t));

    return ret;
}

ADDRINT malloc_get_main_heap_upper_bound(ADDRINT block_addr, size_t blockSize){
    ADDRINT ret = 0;
    ADDRINT main_block = (ADDRINT) ((struct blk_t*) block_addr)->next;
    
    if(main_block)
        ret = main_block + ((struct blk_t*) main_block)->size - 1;
    return ret;
}

ADDRINT malloc_get_block_beginning(ADDRINT addr){
    return addr - HEADER_PADDING;
}

// This function returns a set of pairs representing the portions of an allocated block to be reinitialized on free.
// This is required because in some cases it may lead to false positives reinitializing the whole block
// (e.g. malloc->free->malloc->free, where the second malloc reuses the first freed one).
// On x86_64 architectures, using the glibc implementation of malloc and free, we should also avoid re-initializing
// the first 16 bytes of the payload, as they are written by a call to free in order to keep metadata
// about the freed chunk and may be used by successive calls to malloc.
std::set<std::pair<ADDRINT, size_t>> malloc_mem_to_reinit(ADDRINT block_addr, size_t blockSize){
    std::set<std::pair<ADDRINT, size_t>> ret;
    ret.insert(std::pair<ADDRINT, size_t>(block_addr + HEADER_PADDING, blockSize - HEADER_PADDING));
    return ret;
}