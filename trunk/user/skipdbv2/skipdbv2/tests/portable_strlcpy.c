#include "PortableStrlcpy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "PortableStrlcpy: %s\n", message); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    char full[4] = {'x', 'x', 'x', 'x'};
    char truncated[4] = {'x', 'x', 'x', 'x'};
    char exact[3] = {'x', 'x', 'x'};
    char empty[1] = {'x'};
    char untouched[4] = {'a', 'b', 'c', 'd'};
    char nullSource[4] = {'w', 'x', 'y', 'z'};

    CHECK(PortableStrlcpy(full, "cat", sizeof(full)) == 3,
          "full copy returned the wrong source length");
    CHECK(strcmp(full, "cat") == 0,
          "full copy did not preserve the terminator");

    CHECK(PortableStrlcpy(truncated, "caterpillar", sizeof(truncated)) == 11,
          "truncating copy did not return the full source length");
    CHECK(memcmp(truncated, "cat", 3) == 0 && truncated[3] == '\0',
          "truncating copy did not terminate the destination");

    CHECK(PortableStrlcpy(untouched, "bird", 0) == 4,
          "zero-size copy returned the wrong source length");
    CHECK(untouched[0] == 'a' && untouched[1] == 'b' &&
          untouched[2] == 'c' && untouched[3] == 'd',
          "zero-size copy modified the destination");

    CHECK(PortableStrlcpy(exact, "ab", sizeof(exact)) == 2 &&
          exact[0] == 'a' && exact[1] == 'b' && exact[2] == '\0',
          "exact-fit copy failed");
    CHECK(PortableStrlcpy(empty, "", sizeof(empty)) == 0 && empty[0] == '\0',
          "empty source did not produce an empty destination");

    CHECK(PortableStrlcpy(nullSource, NULL, sizeof(nullSource)) == 0,
          "NULL source was not rejected");
    CHECK(nullSource[0] == 'w' && nullSource[1] == 'x' &&
          nullSource[2] == 'y' && nullSource[3] == 'z',
          "NULL source partially modified the destination");
    CHECK(PortableStrlcpy(NULL, "bird", 0) == 4,
          "NULL zero-size destination did not report source length");
    CHECK(PortableStrlcpy(NULL, "bird", 1) == 0,
          "NULL nonzero-size destination was not rejected");
    CHECK(PortableStrlcpy(NULL, NULL, 0) == 0,
          "two NULL pointers were not rejected safely");

    puts("PortableStrlcpy test passed");
    return EXIT_SUCCESS;
}
