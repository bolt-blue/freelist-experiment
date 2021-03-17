#ifndef MEMORY_H
#define MEMORY_H

#include "utility.h"

/* ========================================================================== */

/// Free List (general purpose) Allocator ///

// uint32_t limits size to 4GiB
// TODO : Given that FreeListChunk is multiple of 8 for alignment
//        'size' should probably just be size_t (or uint64_t)
typedef struct FreeListChunk {
    uint32_t size;              // Includes size of header and any padding
    struct FreeListChunk *next;
    struct FreeListChunk *prev;
} FreeListChunk;

typedef struct MemoryFreeList {
    void *base;
    void *top;
    void *at;

    FreeListChunk *first_free;
} MemoryFreeList;

// TODO : Implement ability to request specific memory alignment
static inline MemoryFreeList MBS_Create_FreeList(void *base, size_t size);
void * my_malloc(size_t size);
void * my_calloc(size_t count, size_t size);
void * my_realloc(void *ptr, size_t size);
void   my_free(void *ptr);

/* ========================================================================== */

#endif /* MBS_MEMORY_H */
