/*
copyright
	Steve Dekorte, 2004
license
	BSD revised
*/

#include "BStreamTag.h"
#include <stdio.h>

BStreamTag BStreamTag_FromUnsignedChar(unsigned char c)
{
    BStreamTag tag;

    tag.isArray = (unsigned int)((c >> 7) & 0x01u);
    tag.type = (unsigned int)((c >> 5) & 0x03u);
    tag.byteCount = (unsigned int)(c & 0x1fu);
    return tag;
}

unsigned char BStreamTag_asUnsignedChar(BStreamTag *self)
{
    if (NULL == self)
        return 0;

    return (unsigned char)(((self->isArray & 0x01u) << 7) |
                           ((self->type & 0x03u) << 5) |
                           (self->byteCount & 0x1fu));
}

BStreamTag BStreamTag_TagArray_type_byteCount_(unsigned int a,
                                               unsigned int t,
                                               unsigned int b)
{
    BStreamTag tag;

    tag.isArray = a & 0x01u;
    tag.type = t & 0x03u;
    tag.byteCount = b & 0x1fu;
    return tag;
}

int BStreamTag_isEqual_(BStreamTag *self, BStreamTag *other)
{
    if (NULL == self || NULL == other)
        return self == other;
    return BStreamTag_asUnsignedChar(self) == BStreamTag_asUnsignedChar(other);
}

void BStreamTag_print(BStreamTag *self)
{
    if (NULL == self)
        return;

    printf("[Tag isArray: %u type: %u byteCount: %u]",
           self->isArray, self->type, self->byteCount);
}

char *BStreamTag_typeName(BStreamTag *self)
{
    if (NULL == self)
        return "UNKNOWN TYPE";

    switch (self->type) {
        case BSTREAM_UNSIGNED_INT:
            return "uint";
        case BSTREAM_SIGNED_INT:
            return "int";
        case BSTREAM_FLOAT:
            return "float";
        case BSTREAM_POINTER:
            return "pointer";
        default:
            return "UNKNOWN TYPE";
    }
}
