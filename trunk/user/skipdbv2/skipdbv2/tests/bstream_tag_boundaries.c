#include "BStreamTag.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "BStreamTag boundary test: %s\n", message); \
        return 1; \
    } \
} while (0)

int main(void)
{
    unsigned int value;
    BStreamTag tag;
    BStreamTag normal;
    BStreamTag masked;

    for (value = 0; value <= 0xffu; ++value) {
        unsigned char wire = (unsigned char)value;
        tag = BStreamTag_FromUnsignedChar(wire);
        CHECK(BStreamTag_asUnsignedChar(&tag) == wire,
              "wire byte did not round-trip exactly");
        CHECK(tag.isArray == ((value >> 7) & 0x01u),
              "array bit decoded incorrectly");
        CHECK(tag.type == ((value >> 5) & 0x03u),
              "type bits decoded incorrectly");
        CHECK(tag.byteCount == (value & 0x1fu),
              "byte-count bits decoded incorrectly");
    }

    normal = BStreamTag_TagArray_type_byteCount_(1u, 2u, 17u);
    CHECK(BStreamTag_asUnsignedChar(&normal) == 0xd1u,
          "normal constructor fields were encoded incorrectly");

    masked = BStreamTag_TagArray_type_byteCount_(2u, 5u, 0x3fu);
    CHECK(masked.isArray == 0u && masked.type == 1u && masked.byteCount == 31u,
          "constructor did not mask inputs to the wire field widths");
    CHECK(BStreamTag_asUnsignedChar(&masked) == 0x3fu,
          "masked constructor fields were encoded incorrectly");

    CHECK(BStreamTag_asUnsignedChar(NULL) == 0u,
          "NULL serialization did not return the zero tag");
    CHECK(BStreamTag_isEqual_(NULL, NULL),
          "two NULL tags were not considered equal");
    CHECK(!BStreamTag_isEqual_(NULL, &normal) &&
          !BStreamTag_isEqual_(&normal, NULL),
          "one NULL tag compared equal to a valid tag");
    CHECK(0 == strcmp(BStreamTag_typeName(NULL), "UNKNOWN TYPE"),
          "NULL type-name query was not safe");
    BStreamTag_print(NULL);

    tag = BStreamTag_FromUnsignedChar(0x00u);
    CHECK(0 == strcmp(BStreamTag_typeName(&tag), "uint"),
          "unsigned type name is incorrect");
    tag = BStreamTag_FromUnsignedChar(0x20u);
    CHECK(0 == strcmp(BStreamTag_typeName(&tag), "int"),
          "signed type name is incorrect");
    tag = BStreamTag_FromUnsignedChar(0x40u);
    CHECK(0 == strcmp(BStreamTag_typeName(&tag), "float"),
          "float type name is incorrect");
    tag = BStreamTag_FromUnsignedChar(0x60u);
    CHECK(0 == strcmp(BStreamTag_typeName(&tag), "pointer"),
          "pointer type name is incorrect");

    puts("BStreamTag boundary test passed");
    return 0;
}
