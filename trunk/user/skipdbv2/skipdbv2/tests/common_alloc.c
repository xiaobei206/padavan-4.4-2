#include "Common.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    unsigned char *tracked;
    unsigned char *resized;
    unsigned char *copy;
    void *overflowed;
    uint32_t endianProbe = 0x01020304U;
    int expectedBigEndian = ((const unsigned char *)&endianProbe)[0] == 0x01;
    uint32_t expectedNetwork = expectedBigEndian ? endianProbe : 0x04030201U;

    if (io_isBigEndian() != expectedBigEndian) {
        fprintf(stderr, "Common endian probe returned the opposite byte order\n");
        return EXIT_FAILURE;
    }
    if (io_uint32InBigEndian(endianProbe) != expectedNetwork) {
        fprintf(stderr, "32-bit big-endian conversion did not match host byte order\n");
        return EXIT_FAILURE;
    }

    io_resetMaxAllocatedBytes();
    tracked = (unsigned char *)io_malloc(16);
    if (NULL == tracked) {
        fprintf(stderr, "tracked allocation failed\n");
        return EXIT_FAILURE;
    }
    memset(tracked, 0x5a, 16);
    if (io_allocs() != 1 || io_allocatedBytes() != 16) {
        fprintf(stderr, "tracked allocation counters are inconsistent\n");
        io_free(tracked);
        return EXIT_FAILURE;
    }

    io_show_mem("tracked allocation diagnostics");
    io_showUnfreed();
    if (io_allocs() != 1 || io_allocatedBytes() != 16 || io_frees() != 0) {
        fprintf(stderr, "allocator diagnostics changed live allocation accounting\n");
        io_free(tracked);
        return EXIT_FAILURE;
    }

    io_free(NULL);
    if (io_frees() != 0) {
        fprintf(stderr, "freeing NULL changed allocation counters\n");
        io_free(tracked);
        return EXIT_FAILURE;
    }

    overflowed = io_calloc((size_t)-1 / 2 + 1, 2);
    if (NULL != overflowed || io_allocs() != 1 || io_allocatedBytes() != 16) {
        fprintf(stderr, "overflowing calloc was not rejected before allocation\n");
        if (overflowed)
            io_free(overflowed);
        io_free(tracked);
        return EXIT_FAILURE;
    }

    resized = (unsigned char *)io_realloc(tracked, (size_t)LONG_MAX);
    if (NULL != resized) {
        io_free(resized);
        fprintf(stderr, "impossibly large realloc unexpectedly succeeded\n");
        return EXIT_FAILURE;
    }
    if (io_allocatedBytes() != 16 || io_frees() != 0 || tracked[0] != 0x5a) {
        fprintf(stderr, "failed realloc lost the original tracked block or counters\n");
        io_free(tracked);
        return EXIT_FAILURE;
    }

    resized = (unsigned char *)io_realloc(tracked, 32);
    if (NULL == resized || io_allocatedBytes() != 32 ||
        io_maxAllocatedBytes() != 32 ||
        resized[0] != 0x5a || resized[15] != 0x5a) {
        fprintf(stderr, "successful realloc did not preserve data and peak accounting\n");
        if (resized)
            io_free(resized);
        else
            io_free(tracked);
        return EXIT_FAILURE;
    }

    copy = (unsigned char *)cpalloc(resized, 32);
    if (NULL == copy || memcmp(copy, resized, 32) != 0) {
        fprintf(stderr, "cpalloc did not preserve its source bytes\n");
        if (copy)
            io_free(copy);
        io_free(resized);
        return EXIT_FAILURE;
    }
    io_free(copy);
    io_free(resized);

    if (io_allocatedBytes() != 0 || io_allocs() != 2 || io_frees() != 2 ||
        io_maxAllocatedBytes() != 64) {
        fprintf(stderr, "allocation accounting did not return to baseline\n");
        return EXIT_FAILURE;
    }

    puts("Common allocation lifecycle test passed");
    return EXIT_SUCCESS;
}
