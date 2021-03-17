#include "utility.h"

void *mem_copy(void *dest, void *src, size_t size)
{
    uint8_t *dest_bytes = (uint8_t *)dest;
    uint8_t *src_bytes = (uint8_t *)src;

    for (uint32_t i=0; i < size; i++) {
        dest_bytes[i] = src_bytes[i];
        // TODO : remove the following assert asap
        ASSERT(dest_bytes[i] == src_bytes[i]);
    }
    return dest;
}
