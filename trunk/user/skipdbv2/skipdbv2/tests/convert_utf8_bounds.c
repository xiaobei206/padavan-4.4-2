#include "ConvertUTF.h"

#include <stdio.h>
#include <stdlib.h>

static int expect_result(const char *label, const UTF8 *source,
                         const UTF8 *end, Boolean expected)
{
    Boolean actual = isLegalUTF8Sequence(source, end);
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n",
                label, (unsigned int)expected, (unsigned int)actual);
        return 0;
    }
    return 1;
}

int main(void)
{
    const UTF8 empty[] = { 0 };
    const UTF8 ascii[] = { 0x41 };
    const UTF8 euro[] = { 0xe2, 0x82, 0xac };
    const UTF8 emoji[] = { 0xf0, 0x9f, 0x92, 0xa9 };
    const UTF8 truncated[] = { 0xe2, 0x82 };
    const UTF8 overlong[] = { 0xc0, 0x80 };
    const UTF8 bad_lead[] = { 0xf5, 0x80, 0x80, 0x80, 0x80 };

    if (!expect_result("NULL source", NULL, ascii + 1, false) ||
        !expect_result("NULL end", ascii, NULL, false) ||
        !expect_result("empty range", empty, empty, false) ||
        !expect_result("ASCII", ascii, ascii + 1, true) ||
        !expect_result("three-byte sequence", euro, euro + 3, true) ||
        !expect_result("four-byte sequence", emoji, emoji + 4, true) ||
        !expect_result("truncated sequence", truncated, truncated + 2, false) ||
        !expect_result("overlong sequence", overlong, overlong + 2, false) ||
        !expect_result("illegal lead byte", bad_lead, bad_lead + 5, false)) {
        return EXIT_FAILURE;
    }

    puts("ConvertUTF sequence-boundary test passed");
    return EXIT_SUCCESS;
}
