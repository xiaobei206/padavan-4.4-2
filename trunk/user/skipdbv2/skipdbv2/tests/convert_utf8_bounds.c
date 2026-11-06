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


static int test_utf32_source_semantics(void)
{
    const UTF32 v[] = {0xd800, 0x110000, 0x41};
    const UTF32 supplementary[] = {0x1f4a9};
    UTF16 out16[4] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd};
    UTF8 out8[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    const UTF32 *s;
    UTF16 *t16;
    UTF8 *t8;
    ConversionResult r;

    s=v; t16=out16;
    r=ConvertUTF32toUTF16(&s,v+3,&t16,out16+4,strictConversion);
    if(r!=sourceIllegal || s!=v || t16!=out16 || out16[0]!=0xaaaa) return 0;
    s=v+1; t16=out16;
    r=ConvertUTF32toUTF16(&s,v+3,&t16,out16+4,strictConversion);
    if(r!=sourceIllegal || s!=v+1 || t16!=out16 || out16[0]!=0xaaaa) {
        fprintf(stderr,"UTF32toUTF16 strict range error consumed the invalid scalar\\n"); return 0;
    }
    s=v; t16=out16;
    r=ConvertUTF32toUTF16(&s,v+3,&t16,out16+4,lenientConversion);
    if(r!=conversionOK || s!=v+3 || t16!=out16+3 || out16[0]!=0xfffd || out16[1]!=0xfffd || out16[2]!=0x41) {
        fprintf(stderr,"UTF32toUTF16 lenient replacement failed\\n"); return 0;
    }
    {
        UTF16 small[1]={0xaaaa}; s=supplementary; t16=small;
        r=ConvertUTF32toUTF16(&s,supplementary+1,&t16,small+1,strictConversion);
        if(r!=targetExhausted || s!=supplementary || t16!=small || small[0]!=0xaaaa) return 0;
    }
    s=v; t8=out8;
    r=ConvertUTF32toUTF8(&s,v+3,&t8,out8+sizeof(out8),strictConversion);
    if(r!=sourceIllegal || s!=v || t8!=out8 || out8[0]!=0xaa) return 0;
    s=v+1; t8=out8;
    r=ConvertUTF32toUTF8(&s,v+3,&t8,out8+sizeof(out8),strictConversion);
    if(r!=sourceIllegal || s!=v+1 || t8!=out8 || out8[0]!=0xaa) {
        fprintf(stderr,"UTF32toUTF8 strict range error consumed the invalid scalar\\n"); return 0;
    }
    s=v; t8=out8;
    r=ConvertUTF32toUTF8(&s,v+3,&t8,out8+sizeof(out8),lenientConversion);
    if(r!=conversionOK || s!=v+3 || t8!=out8+7 || out8[0]!=0xef || out8[1]!=0xbf || out8[2]!=0xbd ||
       out8[3]!=0xef || out8[4]!=0xbf || out8[5]!=0xbd || out8[6]!=0x41) {
        fprintf(stderr,"UTF32toUTF8 lenient replacement failed\\n"); return 0;
    }
    {
        UTF8 small[3]={0xaa,0xaa,0xaa}; s=supplementary; t8=small;
        r=ConvertUTF32toUTF8(&s,supplementary+1,&t8,small+sizeof(small),strictConversion);
        if(r!=targetExhausted || s!=supplementary || t8!=small || small[0]!=0xaa || small[1]!=0xaa || small[2]!=0xaa) {
            fprintf(stderr,"UTF32toUTF8 target exhaustion changed source or output\\n"); return 0;
        }
    }
    return 1;
}


static int test_utf8_conversion_bounds(void)
{
    const UTF8 truncated[] = {0xf0, 0x9f};
    const UTF8 malformed[] = {0xe2, 0x28, 0xa1};
    const UTF8 oversized_lead[] = {0xf8, 0x88, 0x80, 0x80, 0x80, 0x80};
    UTF16 out16[4] = {0xaaaa, 0xbbbb, 0xcccc, 0xdddd};
    UTF32 out32[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    const UTF8 *source8;
    UTF16 *target16;
    UTF32 *target32;
    ConversionResult r;

    source8 = truncated;
    target16 = out16;
    r = ConvertUTF8toUTF16(&source8, truncated + sizeof(truncated),
                           &target16, out16 + 4, strictConversion);
    if (r != sourceExhausted || source8 != truncated || target16 != out16 ||
        out16[0] != 0xaaaa) {
        fprintf(stderr, "ConvertUTF8toUTF16 truncated source changed cursor or output\\n");
        return 0;
    }

    source8 = truncated;
    target32 = out32;
    r = ConvertUTF8toUTF32(&source8, truncated + sizeof(truncated),
                           &target32, out32 + 4, strictConversion);
    if (r != sourceExhausted || source8 != truncated || target32 != out32 ||
        out32[0] != 0xaaaaaaaa) {
        fprintf(stderr, "ConvertUTF8toUTF32 truncated source changed cursor or output\\n");
        return 0;
    }

    source8 = malformed;
    target16 = out16;
    r = ConvertUTF8toUTF16(&source8, malformed + sizeof(malformed),
                           &target16, out16 + 4, strictConversion);
    if (r != sourceIllegal || source8 != malformed || target16 != out16 ||
        out16[0] != 0xaaaa) {
        fprintf(stderr, "ConvertUTF8toUTF16 malformed input was accepted or consumed\\n");
        return 0;
    }

    source8 = malformed;
    target32 = out32;
    r = ConvertUTF8toUTF32(&source8, malformed + sizeof(malformed),
                           &target32, out32 + 4, strictConversion);
    if (r != sourceIllegal || source8 != malformed || target32 != out32 ||
        out32[0] != 0xaaaaaaaa) {
        fprintf(stderr, "ConvertUTF8toUTF32 malformed input was accepted or consumed\\n");
        return 0;
    }

    source8 = oversized_lead;
    target16 = out16;
    r = ConvertUTF8toUTF16(&source8, oversized_lead + sizeof(oversized_lead),
                           &target16, out16 + 4, strictConversion);
    if (r != sourceIllegal || source8 != oversized_lead || target16 != out16) {
        fprintf(stderr, "ConvertUTF8toUTF16 oversized sequence lead was accepted\\n");
        return 0;
    }

    source8 = oversized_lead;
    target32 = out32;
    r = ConvertUTF8toUTF32(&source8, oversized_lead + sizeof(oversized_lead),
                           &target32, out32 + 4, strictConversion);
    if (r != sourceIllegal || source8 != oversized_lead || target32 != out32) {
        fprintf(stderr, "ConvertUTF8toUTF32 oversized sequence lead was accepted\\n");
        return 0;
    }

    return 1;
}


static int test_utf16_source_semantics(void)
{
    const UTF16 high_then_ascii[] = {0xd800, 0x41};
    const UTF16 low_then_ascii[] = {0xdc00, 0x42};
    const UTF16 valid_pair[] = {0xd83d, 0xdca9};
    const UTF16 trailing_high[] = {0xd800};
    UTF32 out32[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    UTF8 out8[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    const UTF16 *source16;
    UTF32 *target32;
    UTF8 *target8;
    ConversionResult r;

    source16 = high_then_ascii; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, high_then_ascii + 2, &target32,
                            out32 + 4, strictConversion);
    if (r != sourceIllegal || source16 != high_then_ascii || target32 != out32) {
        fprintf(stderr, "UTF16toUTF32 strict high surrogate did not preserve cursor\\n");
        return 0;
    }
    source16 = high_then_ascii; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, high_then_ascii + 2, &target32,
                            out32 + 4, lenientConversion);
    if (r != conversionOK || source16 != high_then_ascii + 2 || target32 != out32 + 2 ||
        out32[0] != 0xfffd || out32[1] != 0x41) {
        fprintf(stderr, "UTF16toUTF32 lenient high surrogate replacement failed\\n");
        return 0;
    }

    source16 = low_then_ascii; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, low_then_ascii + 2, &target32,
                            out32 + 4, strictConversion);
    if (r != sourceIllegal || source16 != low_then_ascii || target32 != out32) {
        fprintf(stderr, "UTF16toUTF32 strict low surrogate did not preserve cursor\\n");
        return 0;
    }
    source16 = low_then_ascii; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, low_then_ascii + 2, &target32,
                            out32 + 4, lenientConversion);
    if (r != conversionOK || source16 != low_then_ascii + 2 || target32 != out32 + 2 ||
        out32[0] != 0xfffd || out32[1] != 0x42) {
        fprintf(stderr, "UTF16toUTF32 lenient low surrogate replacement failed\\n");
        return 0;
    }

    source16 = valid_pair; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, valid_pair + 2, &target32,
                            out32 + 4, strictConversion);
    if (r != conversionOK || source16 != valid_pair + 2 || target32 != out32 + 1 ||
        out32[0] != 0x1f4a9) return 0;

    source16 = trailing_high; target32 = out32;
    r = ConvertUTF16toUTF32(&source16, trailing_high + 1, &target32,
                            out32 + 4, lenientConversion);
    if (r != sourceExhausted || source16 != trailing_high || target32 != out32) {
        fprintf(stderr, "UTF16toUTF32 truncated high surrogate was consumed\\n");
        return 0;
    }

    source16 = high_then_ascii; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, high_then_ascii + 2, &target8,
                           out8 + sizeof(out8), strictConversion);
    if (r != sourceIllegal || source16 != high_then_ascii || target8 != out8) {
        fprintf(stderr, "UTF16toUTF8 strict high surrogate did not preserve cursor\\n");
        return 0;
    }
    source16 = high_then_ascii; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, high_then_ascii + 2, &target8,
                           out8 + sizeof(out8), lenientConversion);
    if (r != conversionOK || source16 != high_then_ascii + 2 || target8 != out8 + 4 ||
        out8[0] != 0xef || out8[1] != 0xbf || out8[2] != 0xbd || out8[3] != 0x41) {
        fprintf(stderr, "UTF16toUTF8 lenient high surrogate replacement failed\\n");
        return 0;
    }

    source16 = low_then_ascii; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, low_then_ascii + 2, &target8,
                           out8 + sizeof(out8), strictConversion);
    if (r != sourceIllegal || source16 != low_then_ascii || target8 != out8) {
        fprintf(stderr, "UTF16toUTF8 strict low surrogate did not preserve cursor\\n");
        return 0;
    }
    source16 = low_then_ascii; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, low_then_ascii + 2, &target8,
                           out8 + sizeof(out8), lenientConversion);
    if (r != conversionOK || source16 != low_then_ascii + 2 || target8 != out8 + 4 ||
        out8[0] != 0xef || out8[1] != 0xbf || out8[2] != 0xbd || out8[3] != 0x42) {
        fprintf(stderr, "UTF16toUTF8 lenient low surrogate replacement failed\\n");
        return 0;
    }

    source16 = valid_pair; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, valid_pair + 2, &target8,
                           out8 + sizeof(out8), strictConversion);
    if (r != conversionOK || source16 != valid_pair + 2 || target8 != out8 + 4 ||
        out8[0] != 0xf0 || out8[1] != 0x9f || out8[2] != 0x92 || out8[3] != 0xa9)
        return 0;

    {
        UTF8 small[3] = {0xaa, 0xaa, 0xaa};
        source16 = valid_pair; target8 = small;
        r = ConvertUTF16toUTF8(&source16, valid_pair + 2, &target8,
                               small + sizeof(small), strictConversion);
        if (r != targetExhausted || source16 != valid_pair || target8 != small ||
            small[0] != 0xaa || small[1] != 0xaa || small[2] != 0xaa) {
            fprintf(stderr, "UTF16toUTF8 target exhaustion changed source or output\\n");
            return 0;
        }
    }

    source16 = trailing_high; target8 = out8;
    r = ConvertUTF16toUTF8(&source16, trailing_high + 1, &target8,
                           out8 + sizeof(out8), lenientConversion);
    if (r != sourceExhausted || source16 != trailing_high || target8 != out8) {
        fprintf(stderr, "UTF16toUTF8 truncated high surrogate was consumed\\n");
        return 0;
    }

    return 1;
}

static int test_invalid_buffer_arguments(void)
{
    const UTF8 bytes8[] = { 0x41 };
    const UTF16 bytes16[] = { 0x41 };
    const UTF32 bytes32[] = { 0x41 };
    UTF8 out8[4] = { 0xaa, 0xaa, 0xaa, 0xaa };
    UTF16 out16[4] = { 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa };
    UTF32 out32[4] = { 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa };
    const UTF8 *source8 = bytes8;
    const UTF16 *source16 = bytes16;
    const UTF32 *source32 = bytes32;
    const UTF8 *nullSource8 = NULL;
    const UTF16 *nullSource16 = NULL;
    const UTF32 *nullSource32 = NULL;
    UTF8 *target8 = out8;
    UTF16 *target16 = out16;
    UTF32 *target32 = out32;
    UTF8 *nullTarget8 = NULL;
    UTF16 *nullTarget16 = NULL;
    UTF32 *nullTarget32 = NULL;

#define EXPECT_INVALID(label, expression) do { \
    if ((expression) != sourceIllegal) { \
        fprintf(stderr, "%s accepted invalid buffer arguments\n", label); \
        return 0; \
    } \
} while (0)

    EXPECT_INVALID("ConvertUTF32toUTF16 NULL source handle",
        ConvertUTF32toUTF16(NULL, bytes32 + 1, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF16 NULL source end",
        ConvertUTF32toUTF16(&source32, NULL, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF16 NULL source cursor",
        ConvertUTF32toUTF16(&nullSource32, bytes32 + 1, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF16 NULL target handle",
        ConvertUTF32toUTF16(&source32, bytes32 + 1, NULL, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF16 NULL target end",
        ConvertUTF32toUTF16(&source32, bytes32 + 1, &target16, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF16 NULL target cursor",
        ConvertUTF32toUTF16(&source32, bytes32 + 1, &nullTarget16, out16 + 4, strictConversion));

    EXPECT_INVALID("ConvertUTF16toUTF32 NULL source handle",
        ConvertUTF16toUTF32(NULL, bytes16 + 1, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF32 NULL source end",
        ConvertUTF16toUTF32(&source16, NULL, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF32 NULL source cursor",
        ConvertUTF16toUTF32(&nullSource16, bytes16 + 1, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF32 NULL target handle",
        ConvertUTF16toUTF32(&source16, bytes16 + 1, NULL, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF32 NULL target end",
        ConvertUTF16toUTF32(&source16, bytes16 + 1, &target32, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF32 NULL target cursor",
        ConvertUTF16toUTF32(&source16, bytes16 + 1, &nullTarget32, out32 + 4, strictConversion));

    EXPECT_INVALID("ConvertUTF16toUTF8 NULL source handle",
        ConvertUTF16toUTF8(NULL, bytes16 + 1, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF8 NULL source end",
        ConvertUTF16toUTF8(&source16, NULL, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF8 NULL source cursor",
        ConvertUTF16toUTF8(&nullSource16, bytes16 + 1, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF8 NULL target handle",
        ConvertUTF16toUTF8(&source16, bytes16 + 1, NULL, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF8 NULL target end",
        ConvertUTF16toUTF8(&source16, bytes16 + 1, &target8, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF16toUTF8 NULL target cursor",
        ConvertUTF16toUTF8(&source16, bytes16 + 1, &nullTarget8, out8 + 4, strictConversion));

    EXPECT_INVALID("ConvertUTF8toUTF16 NULL source handle",
        ConvertUTF8toUTF16(NULL, bytes8 + 1, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF16 NULL source end",
        ConvertUTF8toUTF16(&source8, NULL, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF16 NULL source cursor",
        ConvertUTF8toUTF16(&nullSource8, bytes8 + 1, &target16, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF16 NULL target handle",
        ConvertUTF8toUTF16(&source8, bytes8 + 1, NULL, out16 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF16 NULL target end",
        ConvertUTF8toUTF16(&source8, bytes8 + 1, &target16, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF16 NULL target cursor",
        ConvertUTF8toUTF16(&source8, bytes8 + 1, &nullTarget16, out16 + 4, strictConversion));

    EXPECT_INVALID("ConvertUTF32toUTF8 NULL source handle",
        ConvertUTF32toUTF8(NULL, bytes32 + 1, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF8 NULL source end",
        ConvertUTF32toUTF8(&source32, NULL, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF8 NULL source cursor",
        ConvertUTF32toUTF8(&nullSource32, bytes32 + 1, &target8, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF8 NULL target handle",
        ConvertUTF32toUTF8(&source32, bytes32 + 1, NULL, out8 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF8 NULL target end",
        ConvertUTF32toUTF8(&source32, bytes32 + 1, &target8, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF32toUTF8 NULL target cursor",
        ConvertUTF32toUTF8(&source32, bytes32 + 1, &nullTarget8, out8 + 4, strictConversion));

    EXPECT_INVALID("ConvertUTF8toUTF32 NULL source handle",
        ConvertUTF8toUTF32(NULL, bytes8 + 1, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF32 NULL source end",
        ConvertUTF8toUTF32(&source8, NULL, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF32 NULL source cursor",
        ConvertUTF8toUTF32(&nullSource8, bytes8 + 1, &target32, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF32 NULL target handle",
        ConvertUTF8toUTF32(&source8, bytes8 + 1, NULL, out32 + 4, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF32 NULL target end",
        ConvertUTF8toUTF32(&source8, bytes8 + 1, &target32, NULL, strictConversion));
    EXPECT_INVALID("ConvertUTF8toUTF32 NULL target cursor",
        ConvertUTF8toUTF32(&source8, bytes8 + 1, &nullTarget32, out32 + 4, strictConversion));

#undef EXPECT_INVALID

    if (ConvertUTF32toUTF16(&source32, bytes32, &target16, out16, strictConversion) != conversionOK ||
        ConvertUTF16toUTF32(&source16, bytes16, &target32, out32, strictConversion) != conversionOK ||
        ConvertUTF16toUTF8(&source16, bytes16, &target8, out8, strictConversion) != conversionOK ||
        ConvertUTF8toUTF16(&source8, bytes8, &target16, out16, strictConversion) != conversionOK ||
        ConvertUTF32toUTF8(&source32, bytes32, &target8, out8, strictConversion) != conversionOK ||
        ConvertUTF8toUTF32(&source8, bytes8, &target32, out32, strictConversion) != conversionOK) {
        fprintf(stderr, "valid empty ConvertUTF ranges were rejected\\n");
        return 0;
    }

    if (source8 != bytes8 || source16 != bytes16 || source32 != bytes32 ||
        target8 != out8 || target16 != out16 || target32 != out32 ||
        nullSource8 != NULL || nullSource16 != NULL || nullSource32 != NULL ||
        nullTarget8 != NULL || nullTarget16 != NULL || nullTarget32 != NULL ||
        out8[0] != 0xaa || out16[0] != 0xaaaa || out32[0] != 0xaaaaaaaa) {
        fprintf(stderr, "invalid ConvertUTF arguments changed caller cursors or output\\n");
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

    if (!test_invalid_buffer_arguments()) return EXIT_FAILURE;

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

    if (!test_utf32_source_semantics() || !test_utf8_conversion_bounds() || !test_utf16_source_semantics()) return EXIT_FAILURE;

    puts("ConvertUTF sequence-boundary test passed");
    return EXIT_SUCCESS;
}
