/*
copyright
	Steve Dekorte, 2004
license
	BSD revised
*/

#include "BStreamTag.h"
#include <stdio.h>

/*
unsigned int isArray   : 1;
unsigned int type      : 2;
unsigned int byteCount : 5;
*/

BStreamTag BStreamTag_FromUnsignedChar(unsigned char c)
{
	/* Extract wire-format fields explicitly; bit-field assignments do not
	 * mask the shifted integer expressions used by the previous version. */
	BStreamTag t;
	t.isArray = (c >> 7) & 0x01;
	t.type = (c >> 5) & 0x03;
	t.byteCount = c & 0x1f;
	return t;
}

unsigned char BStreamTag_asUnsignedChar(BStreamTag *self)
{
	BStreamTag t;
	unsigned char c = 0;
	if (NULL == self)
		return 0;
	t = *self;
	c = c | t.isArray << 7;
	c = c | t.type << 5;
	c = c | t.byteCount;
	return c;
}

// -----------------------------------------------------

BStreamTag BStreamTag_TagArray_type_byteCount_(unsigned int a, unsigned int t, unsigned int b)
{
	BStreamTag self;
	self.isArray = a;
	self.type = t;
	self.byteCount = b;
	return self;
}

int BStreamTag_isEqual_(BStreamTag *self, BStreamTag *other)
{
	if (NULL == self || NULL == other)
		return 0;
	return BStreamTag_asUnsignedChar(self) == BStreamTag_asUnsignedChar(other);
}

void BStreamTag_print(BStreamTag *self)
{
	if (NULL == self)
		return;
	printf("[Tag ");
	printf("isArray: %i ", self->isArray);
	printf("type: %i ", self->type);
	printf("byteCount: %i", self->byteCount);
	printf("]");
}

char *BStreamTag_typeName(BStreamTag *self)
{
	if (NULL == self)
		return "UNKNOWN TYPE";
	switch (self->type)
	{
		case BSTREAM_UNSIGNED_INT:
			return "uint";
		case BSTREAM_SIGNED_INT:
			return "int";
		case BSTREAM_FLOAT:
			return "float";
		case BSTREAM_POINTER:
			return "pointer";
	}

	return "UNKNOWN TYPE";
}
