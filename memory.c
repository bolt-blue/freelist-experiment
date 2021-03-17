#define _GNU_SOURCE
#include "memory.h"
#include <sys/mman.h>   // mmap NOTE: -D_GNU_SOURCE is required to use MAP_ANONYMOUS

extern void *__libc_malloc(size_t size);
extern void *__libc_calloc(size_t nmemb, size_t size);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_free(void *ptr);

static void *my_malloc_hook(size_t size, const void *caller);
static void *my_calloc_hook(size_t nmemb, size_t size, const void *caller);
static void *my_realloc_hook(void * ptr, size_t size, const void *caller);
static void  my_free_hook(void *ptr, const void *caller);

int g_malloc_hook_active = 1;

/* ========================================================================== */

void *
malloc(size_t size)
{
    void *caller = __builtin_return_address(0);
    if (g_malloc_hook_active)
        return my_malloc_hook(size, caller);
    return __libc_malloc(size);
}

void *calloc(size_t nmemb, size_t size)
{
    void *caller = __builtin_return_address(0);
    if (g_malloc_hook_active)
        return my_calloc_hook(nmemb, size, caller);
    return __libc_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    void *caller = __builtin_return_address(0);
    if (g_malloc_hook_active)
        return my_realloc_hook(ptr, size, caller);
    return __libc_realloc(ptr, size);
}

void free(void *ptr)
{
    void *caller = __builtin_return_address(0);
    if (g_malloc_hook_active)
        return my_free_hook(ptr, caller);
    __libc_free(ptr);
}

/* ========================================================================== */

struct ptr_ref {
    void *system;
    void *internal;
};

MemoryFreeList g_mbs_arena;
struct ptr_refs
{
    uint32_t capacity;
    uint32_t count;
    struct ptr_ref *array;
} g_ptr_refs;

// NOTE : Having ALIGN == 8 gives us the 3 LSB's for use as flags
#if __STDC_VERSION__ >= 201112L
#include <stddef.h>
#endif
union Align {
#if __STDC_VERSION__ >= 201112L
    max_align_t maxalign;
#elif __STDC_VERSION__ >= 199901L
    long long ll;
#endif
    long double ld;
    long l;
    double d;
    char *p;
    int (*f)(void);
};
#define ALIGN sizeof(union Align)

INTERNAL int Consolidate_(FreeListChunk *chunk);

void init_test_arena(void)
{
    g_mbs_arena = MBS_Create_FreeList(0, MEGABYTE(64));
    // Dirtily hard-coded array for tracking system allocations against internal
    g_ptr_refs.array = my_malloc(100000);
    g_ptr_refs.capacity = 100000;
    g_ptr_refs.count = 0;
}
void push_ptr_ref(void *system, void *internal)
{
    if (!system)
    {
        return;
    }
    ASSERT(g_ptr_refs.count + 1 <= g_ptr_refs.capacity);
    g_ptr_refs.array[g_ptr_refs.count].system = system;
    g_ptr_refs.array[g_ptr_refs.count].internal = internal;
    g_ptr_refs.count++;
}
void * pop_ptr_ref(void *system)
{
    if (!system)
    {
        return NULL;
    }
    void *ret;
    for (uint32_t i = 0; i < g_ptr_refs.count; i++)
    {
        if (g_ptr_refs.array[i].system == system)
        {
            ret = g_ptr_refs.array[i].internal;
            g_ptr_refs.array[i] = g_ptr_refs.array[g_ptr_refs.count];
            g_ptr_refs.count--;
            break;
        }
    }
    return ret;
}

static inline MemoryFreeList
MBS_Create_FreeList(void *base, size_t size)
{
    if (!base)
    {
        base = mmap(0, size,
                    PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE,
                    -1, 0);
    }

    MemoryFreeList ret = {
        .base = base,
        .at = base,
        .top = base + size,
        .first_free = NULL,
    };

    return ret;
}

/*
 * Memory chunks will always be size_t aligned at head and tail
 * Any single chunk is capped at 4GiB (as 'size' is stored as uint32_t)
 */
void *
my_malloc(size_t size)
{
    if (size <= 0)
    {
        return NULL;
    }

    size_t total_size = size + sizeof(FreeListChunk);
    total_size += (ALIGN - (total_size & (ALIGN - 1))) & (ALIGN - 1);

    ASSERT(total_size <= UINT32_MAX);

    FreeListChunk *free_chunk = g_mbs_arena.first_free;
    FreeListChunk **free_head = &(g_mbs_arena.first_free);

    while (free_chunk)
    {
        // First-fit policy
        // NOTE : Disregard 3 LSB's (reserved for flags)
        if ((free_chunk->size & 0xfffffff8) >= total_size)
        {
            *free_head = free_chunk->next;

            size_t remainder = (free_chunk->size & 0xfffffff8) - total_size;
            if (remainder > sizeof(FreeListChunk))
            {
                // Set a fake chunk header size and just use my_free()
                FreeListChunk *tmp = (void *)((char *)free_chunk + total_size);
                tmp->size = remainder;

                // Prevent the freak chance that this chunk would be re-consolidated
                uint32_t *tail = (uint32_t *)((char *)tmp - sizeof(uint32_t));
                *tail = 0;

                my_free((char *)tmp + sizeof(FreeListChunk));

                free_chunk->size = total_size;
            }

            // Mark chunk as occupied
            free_chunk->size |= 1;

            return (char *)free_chunk + sizeof(FreeListChunk);
        }

        free_head = &(free_chunk->next);
        free_chunk = free_chunk->next;
    }


    if (g_mbs_arena.at + total_size <= g_mbs_arena.top)
    {
        FreeListChunk *chunk = g_mbs_arena.at;
        chunk->size = total_size;
        chunk->next = NULL;
        chunk->prev = NULL;

        g_mbs_arena.at += total_size;

        // Mark chunk as occupied
        chunk->size |= 1;

        return (char *)chunk + sizeof(FreeListChunk);
    }
    else if (!g_mbs_arena.base)
    {
        init_test_arena();

        return my_malloc(size);
    }

    // Out of memory
    fprintf(stderr, "[ERROR] Out of memory!\n");

    return NULL;
}

void *
my_calloc(size_t count, size_t size)
{
    size_t total_size = size * count;
    void *result = my_malloc(total_size);

    if (result)
    {
        char *cur = result;
        char *end = result + total_size;

        for (; cur != end; cur++)
        {
            *cur = 0;
        }
    }

    return result;
}

void *
my_realloc(void *ptr, size_t size)
{
    // TODO :
    // - Check if there is a suitable free chunk directly after this one

    void *new = my_malloc(size);
    if (!new)
    {
        return NULL;
    }

    if (!ptr)
    {
        return new;
    }

    FreeListChunk *head = ptr - sizeof(FreeListChunk);
    uint32_t old_size = head->size - sizeof(FreeListChunk);

    mem_copy(new, ptr, old_size);

    my_free(ptr);

    return new;
}

void
my_free(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    FreeListChunk *chunk = (void *)((char *)ptr - sizeof(FreeListChunk));

    ASSERT((char *)chunk >= (char *)g_mbs_arena.base && 
           (char *)chunk <  (char *)g_mbs_arena.at);

    // Mark chunk as free (clear occupied bit)
    chunk->size &= 0xfffffffe;

    FreeListChunk *chk = (void *)((char *)chunk + chunk->size);
    if ((char *)chk < (char *)g_mbs_arena.at)
    {
        // Likely fallible (in)sanity check
        if ((void *)chk >= g_mbs_arena.base && 
            (void *)chk <  g_mbs_arena.at && 
            chk->size > 0 && 
            (chk->next == NULL || 
             ((void *)chk->next >= g_mbs_arena.base && 
              (void *)chk->next <  g_mbs_arena.at)) && 
            (chk->prev == NULL || 
             ((void *)chk->prev >= g_mbs_arena.base && 
              (void *)chk->prev <  g_mbs_arena.at)) && 
            Consolidate_(chk))
        {
            chunk->size += chk->size;
        }
    }

    if ((char *)chunk > (char *)g_mbs_arena.base)
    {
        uint32_t prev_sz = *(uint32_t *)((char *)chunk - sizeof(uint32_t));
        // Mild (in)sanity check
        prev_sz *= ((prev_sz & 0x00000007) == 0);
        prev_sz *= (((char *)chunk - prev_sz >= (char *)g_mbs_arena.base) &&
                    ((char *)chunk - prev_sz <  (char *)g_mbs_arena.at));
        if (prev_sz)
        {
            chk = (FreeListChunk *)((char *)chunk - prev_sz);

            // Further, yet likely fallible, (in)sanity check
            if (chk->size == prev_sz && 
                (chk->next == NULL || 
                 ((void *)chk->next >= g_mbs_arena.base && 
                  (void *)chk->next <  g_mbs_arena.at)) && 
                (chk->prev == NULL || 
                 ((void *)chk->prev >= g_mbs_arena.base && 
                  (void *)chk->prev <  g_mbs_arena.at)) && 
                Consolidate_(chk))
            {
                chk->size += chunk->size;
                chunk = chk;
            }
        }
    }

    // Leave size reference at tail and mask out any flag bits
    // Alignment padding means there will always be enough space for this
    uint32_t *tail = (uint32_t *)((char *)chunk + chunk->size - sizeof(uint32_t));
    *tail = chunk->size &= 0xfffffff8;

    chunk->next = g_mbs_arena.first_free;
    if (g_mbs_arena.first_free) {g_mbs_arena.first_free->prev = chunk;}
    g_mbs_arena.first_free = chunk;
    chunk->prev = NULL;
}

int
Consolidate_(FreeListChunk *chunk)
{
    if ((chunk->size & 1) == 1)
    {
        // Chunk is occupied
        return 0;
    }

    // Remove chunk from the free list
    if (chunk->prev)
    {
        chunk->prev->next = chunk->next;
    }
    else
    {
        // We're at the head of the free list - Update!
        g_mbs_arena.first_free = chunk->next;
    }

    if (chunk->next) {chunk->next->prev = chunk->prev;}

    return 1;
}

/* ========================================================================== */

void *my_malloc_hook(size_t size, const void *caller)
{
    void *result;

    // Deactivate hooks for logging
    g_malloc_hook_active = 0;

    printf("===============================================================\n");
    printf("caller: %p\n", caller);

    void *internal_result = my_malloc(size);
    printf("malloc'd size %ld at %p [internal]\n", size, internal_result);

    result = malloc(size);
    printf("malloc'd size %ld at %p [system]\n", size, result);

    g_malloc_hook_active = 1;

    push_ptr_ref(result, internal_result);

    return result;
}

void *my_calloc_hook(size_t nmemb, size_t size, const void *caller)
{
    void *result;

    // Deactivate hooks for logging
    g_malloc_hook_active = 0;

    printf("===============================================================\n");
    printf("caller: %p\n", caller);

    void *internal_result = my_calloc(nmemb, size);
    printf("calloc'd size %ld at %p [internal]\n", size, internal_result);

    result = calloc(nmemb, size);
    printf("calloc'd size %ld at %p [system]\n", size, result);

    push_ptr_ref(result, internal_result);

    g_malloc_hook_active = 1;

    return result;
}

void *my_realloc_hook(void * ptr, size_t size, const void *caller)
{
    void *result;

    // Deactivate hooks for logging
    g_malloc_hook_active = 0;

    printf("===============================================================\n");
    printf("caller: %p\n", caller);

    void *ptr_internal = pop_ptr_ref(ptr);
    void *internal_result = my_realloc(ptr_internal, size);
    printf("realloc'd size %ld at %p [internal]\n", size, internal_result);

    result = realloc(ptr, size);
    printf("realloc'd size %ld at %p [system]\n", size, result);

    push_ptr_ref(result, internal_result);

    g_malloc_hook_active = 1;

    return result;
}

void my_free_hook(void *ptr, const void *caller)
{
    // Deactivate hooks for logging
    g_malloc_hook_active = 0;

    printf("===============================================================\n");
    printf("caller: %p\n", caller);

    void *ptr_internal = pop_ptr_ref(ptr);
    printf ("free pointer  %p [internal]\n", ptr_internal);
    my_free(ptr_internal);
    printf ("> SUCCESS\n");

    printf ("free pointer  %p [system]\n", ptr);
    free(ptr);
    printf ("> SUCCESS\n");

    g_malloc_hook_active = 1;
}
