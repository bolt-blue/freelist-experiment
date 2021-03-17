#ifndef UTILITY_H
#define UTILITY_H

#include <stdlib.h>     // size_t
#include <stdint.h>     // uints, ...
#include <stdio.h>      // fprintf

/* ========================================================================== */

#define INTERNAL static
#define LOCAL_PERSIST static
#define GLOBAL_VARIABLE static

#define KILOBYTE(v) ((v)*1024LL)
#define MEGABYTE(v) (KILOBYTE(v)*1024LL)
#define GIGABYTE(v) (MEGABYTE(v)*1024LL)
#define TERABYTE(v) (GIGABYTE(v)*1024LL)

typedef uint8_t     byte;

#ifdef NDEBUG
    #define ASSERT(exp)
    #define ASSERT_WITH_MSG(exp, fmt, ...)
#else
    // TODO : change ASSERT to do more than just crash
    #define ASSERT(exp)                 \
        if (!(exp)) {                   \
            fprintf(stderr, "[ASSERTION] (%s:%d:%s)\n", __FILE__, __LINE__, __func__);  \
            *(int *volatile)0 = 0;              \
        }
    #define ASSERT_WITH_MSG(exp, fmt, ...)      \
        if (!(exp)) {                           \
            fprintf(stderr, "[ASSERTION] " fmt " (%s:%d:%s)\n", __VA_ARGS__, __FILE__, __LINE__, __func__); \
        }
#endif

// TODO : should use exit(1) instead of __builtin_trap() ?
#define THROW(fmt) \
    fprintf(stderr, "[THROW] " fmt " (%s:%d:%s)\n", __FILE__, __LINE__, __func__); \
    __builtin_trap();
#define THROWX(fmt, ...) \
{ \
    fprintf(stderr, "[THROW] " fmt " (%s:%d:%s)\n", __VA_ARGS__, __FILE__, __LINE__, __func__); \
    __builtin_trap(); \
}

/* ========================================================================== */

void *mem_copy(void *dest, void *src, size_t size);

#endif /* UTILITY_H */
