/*
copyright
	Steve Dekorte, 2004
license
	BSD revised
*/

#include "Base.h"

//#define BStream_C
#include "BStream.h"
//#undef BStream_C

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#define BSTREAM_IS_BIG_ENDIAN 1

BStream *BStream_new(void)
{
	BStream *self = (BStream *)io_calloc(1, sizeof(BStream));
	if (NULL == self)
		return NULL;
	self->ba = UArray_new();
	self->ownsUArray = 1;
	self->tmp = UArray_new();
	self->errorBa = UArray_new();
	self->typeBuf = (unsigned char *)io_calloc(1, 512);
	if (NULL == self->ba || NULL == self->tmp ||
		NULL == self->errorBa || NULL == self->typeBuf) {
		if (self->ba) UArray_free(self->ba);
		if (self->tmp) UArray_free(self->tmp);
		if (self->errorBa) UArray_free(self->errorBa);
		io_free(self->typeBuf);
		io_free(self);
		return NULL;
	}
	self->index = 0;
	self->flipEndian = 0;
	return self;
}

BStream *BStream_clone(BStream *self)
{
	BStream *child;
	if (NULL == self || NULL == self->ba)
		return NULL;
	child = BStream_new();
	if (NULL == child)
		return NULL;
	UArray_copy_(child->ba, self->ba);
	child->index = self->index;
	child->flipEndian = self->flipEndian;
	return child;
}

void BStream_free(BStream *self)
{
	if (NULL == self)
		return;
	if (self->ownsUArray && self->ba) UArray_free(self->ba);
	if (self->tmp) UArray_free(self->tmp);
	if (self->errorBa) UArray_free(self->errorBa);
	io_free(self->typeBuf);
	io_free(self);
}

void BStream_error_(BStream *self, const char *s)
{
	if (self && self->errorBa && s)
		UArray_setCString_(self->errorBa, s);
}

char *BStream_error(BStream *self)
{
	return self && self->errorBa
		? (char *)UArray_bytes(self->errorBa) : NULL;
}

char *BStream_errorString(BStream *self)
{
	return BStream_error(self);
}

void BStream_setUArray_(BStream *self, UArray *ba)
{
	if (NULL == self || NULL == ba || ba->itemSize != 1 ||
		(ba->size > 0 && NULL == ba->data))
		return;
	if (self->ba == ba) {
		self->index = 0;
		if (self->errorBa)
			UArray_setSize_(self->errorBa, 0);
		return;
	}
	if (self->ownsUArray && self->ba)
		UArray_free(self->ba);
	self->ba = ba;
	self->ownsUArray = 0;
	self->index = 0;
	if (self->errorBa)
		UArray_setSize_(self->errorBa, 0);
}


void BStream_setData_length_(BStream *self, unsigned char *data, size_t length)
{
	if (NULL == self || NULL == self->ba || (length > 0 && NULL == data) ||
		length == (size_t)-1)
		return;
	UArray_setData_type_size_copy_(self->ba, data, CTYPE_uint8_t, length, 1);
	if (self->ba->itemSize != 1 || self->ba->size != length ||
		(length > 0 && NULL == self->ba->data)) {
		BStream_error_(self, "unable to set binary stream data");
		return;
	}
	self->index = 0;
	if (self->errorBa)
		UArray_setSize_(self->errorBa, 0);
}


UArray *BStream_byteArray(BStream *self)
{
	return self ? self->ba : NULL;
}

void BStream_empty(BStream *self)
{
	if (NULL == self || NULL == self->ba)
		return;
	self->index = 0;
	UArray_setSize_(self->ba, 0);
	if (self->errorBa)
		UArray_setSize_(self->errorBa, 0);
}

int BStream_isEmpty(BStream *self)
{
	return self && self->ba && UArray_size(self->ba) == 0;
}

// writing --------------------------------------

void BStream_writeByte_(BStream *self, unsigned char v)
{
	BStream_writeUint8_(self, v);
}

static int BStream_appendBytes_(BStream *self, const unsigned char *bytes, size_t length)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		self->ba->itemSize != 1 || UArray_size(self->errorBa) != 0 ||
		(length > 0 && NULL == bytes)) {
		BStream_error_(self, "invalid binary stream write");
		return 0;
	}
	oldSize = UArray_size(self->ba);
	if (length > (size_t)-1 - oldSize || oldSize + length == (size_t)-1) {
		BStream_error_(self, "binary stream size overflow");
		return 0;
	}
	UArray_appendBytes_size_(self->ba, (unsigned char *)bytes, length);
	if (UArray_size(self->ba) != oldSize + length) {
		BStream_error_(self, "unable to grow binary stream");
		return 0;
	}
	self->index = oldSize + length;
	return 1;
}

static void BStream_rollbackWrite_(BStream *self, size_t oldSize)
{
	if (NULL == self || NULL == self->ba || self->ba->itemSize != 1)
		return;
	if (UArray_size(self->ba) > oldSize) {
		UArray_setSize_(self->ba, oldSize);
		if (UArray_size(self->ba) != oldSize) {
			self->ba->size = oldSize;
			if (self->ba->data)
				self->ba->data[oldSize] = 0;
			UArray_changed(self->ba);
		}
	}
	self->index = oldSize;
}

void BStream_writeUint8_(BStream *self, uint8_t v)
{
	if (NULL == self || NULL == self->errorBa)
		return;
	BStream_appendBytes_(self, &v, 1);
}


static void reverseBytes(unsigned char *d, size_t length)
{
	size_t a = 0;
	size_t b = length - 1;

	while ( a < b)
	{
		unsigned char c = d[a];

		d[a] = d[b];
		d[b] = c;
		a ++;
		b --;
	}
}

void BStream_writeNumber_size_(BStream *self, unsigned char *v, size_t length)
{
	if (NULL == self || NULL == self->ba || NULL == self->typeBuf ||
		NULL == self->errorBa || (length > 0 && NULL == v) || length > 512) {
		BStream_error_(self, "invalid binary number write");
		return;
	}
	if (UArray_size(self->errorBa) != 0)
		return;
	if (length == 0)
		return;
	memcpy(self->typeBuf, v, length);
	if (self->flipEndian && length > 1)
		reverseBytes(self->typeBuf, length);
	BStream_appendBytes_(self, self->typeBuf, length);
}


void BStream_writeData_length_(BStream *self, const unsigned char *data, size_t length)
{
	if (NULL == self || NULL == self->errorBa)
		return;
	BStream_appendBytes_(self, data, length);
}


void BStream_writeInt32_(BStream *self, int32_t v)
{
	BStream_writeNumber_size_(self, (unsigned char *)(&v), sizeof(int32_t));
}

void BStream_writeUint32_(BStream *self, uint32_t v)
{
	BStream_writeNumber_size_(self, (unsigned char *)(&v), sizeof(uint32_t));
}

#if !defined(__SYMBIAN32__)
void BStream_writeInt64_(BStream *self, int64_t v)
{
	BStream_writeNumber_size_(self, (unsigned char *)(&v), sizeof(int64_t));
}
#endif

void BStream_writeDouble_(BStream *self, double v)
{
	BStream_writeNumber_size_(self, (unsigned char *)(&v), sizeof(double));
}

void BStream_writeCString_(BStream *self, const char *s)
{
	size_t length, oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	if (NULL == s) {
		BStream_error_(self, "invalid binary stream string");
		return;
	}
	length = strlen(s);
	oldSize = UArray_size(self->ba);
	if (length > INT32_MAX || oldSize > (size_t)-1 - 4 ||
		length > (size_t)-1 - oldSize - 4 || oldSize + 4 + length == (size_t)-1) {
		BStream_error_(self, "string too large for binary stream");
		return;
	}
	BStream_writeInt32_(self, (int32_t)length);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeData_length_(self, (const unsigned char *)s, length);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


void BStream_writeUArray_(BStream *self, UArray *ba)
{
	size_t length, oldSize;
	const unsigned char *bytes;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		NULL == ba || UArray_size(self->errorBa) != 0)
		return;
	if (self->ba == ba || ba->itemSize == 0 ||
		ba->size > (size_t)-1 / ba->itemSize ||
		(ba->size > 0 && NULL == ba->data)) {
		BStream_error_(self, "invalid binary stream array");
		return;
	}
	length = UArray_sizeInBytes(ba);
	oldSize = UArray_size(self->ba);
	if (self->ba->itemSize != 1 || length > INT32_MAX ||
		oldSize > (size_t)-1 - 4 ||
		length > (size_t)-1 - oldSize - 4 ||
		oldSize + 4 + length == (size_t)-1) {
		BStream_error_(self, "array too large for binary stream");
		return;
	}
	bytes = UArray_bytes(ba);
	BStream_writeInt32_(self, (int32_t)length);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeData_length_(self, bytes, length);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


// reading --------------------------------------

unsigned char BStream_readByte(BStream *self)
{
	return BStream_readUint8(self);
}

uint8_t BStream_readUint8(BStream *self)
{
	const unsigned char *bytes;
	if (NULL == self || NULL == self->ba) {
		BStream_error_(self, "invalid binary stream");
		return 0;
	}
	if (self->index < UArray_size(self->ba)) {
		bytes = UArray_bytes(self->ba);
		if (NULL == bytes) {
			BStream_error_(self, "missing binary stream data");
			return 0;
		}
		return bytes[self->index++];
	}
	BStream_error_(self, "truncated binary stream byte");
	return 0;
}

void BStream_readNumber_size_(BStream *self, unsigned char *v, int size)
{
	size_t available;
	const uint8_t *b;
	if (NULL == v || size < 0)
		return;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		self->ba->itemSize != 1) {
		BStream_error_(self, "invalid binary stream read");
		memset(v, 0, (size_t)size);
		return;
	}
	if (UArray_size(self->errorBa) != 0) {
		memset(v, 0, (size_t)size);
		return;
	}
	available = UArray_size(self->ba);
	if (self->index <= available && (size_t)size <= available - self->index) {
		b = UArray_bytes(self->ba);
		if (size > 0 && NULL == b)
			goto invalid;
		if (size > 0)
			memcpy(v, b + self->index, (size_t)size);
		if (self->flipEndian && size > 1)
			reverseBytes(v, (size_t)size);
		self->index += (size_t)size;
		return;
	}
invalid:
	BStream_error_(self, "truncated binary stream");
	memset(v, 0, (size_t)size);
}


uint32_t BStream_readUint32(BStream *self)
{
	uint32_t v = 0;
	BStream_readNumber_size_(self, (unsigned char *)(&v), sizeof(uint32_t));
	return v;
}

int32_t BStream_readInt32(BStream *self)
{
	int32_t v = 0;
	BStream_readNumber_size_(self, (unsigned char *)(&v), sizeof(int32_t));
	return v;
}

#if !defined(__SYMBIAN32__)
int64_t BStream_readInt64(BStream *self)
{
	int64_t v = 0;
	BStream_readNumber_size_(self, (unsigned char *)(&v), sizeof(int64_t));
	return v;
}
#endif

double BStream_readDouble(BStream *self)
{
	double v = 0.0;
	BStream_readNumber_size_(self, (unsigned char *)(&v), sizeof(double));
	return v;
}

unsigned char *BStream_readDataOfLength_(BStream *self, size_t length)
{
	size_t available;
	const unsigned char *bytes;
	if (NULL == self || NULL == self->ba || self->ba->itemSize != 1) {
		BStream_error_(self, "invalid binary stream data read");
		return NULL;
	}
	available = UArray_size(self->ba);
	if (self->index > available || length > available - self->index) {
		BStream_error_(self, "truncated binary stream data");
		return NULL;
	}
	bytes = UArray_bytes(self->ba);
	if (length > 0 && NULL == bytes) {
		BStream_error_(self, "missing binary stream data");
		return NULL;
	}
	if (length == 0)
		return bytes ? (unsigned char *)(bytes + self->index) : NULL;
	self->index += length;
	return (unsigned char *)(bytes + self->index - length);
}


void BStream_readUArray_(BStream *self, UArray *b)
{
	int32_t encoded_size;
	unsigned char *data;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		NULL == b || b == self->ba || self->ba->itemSize != 1 ||
		UArray_size(self->errorBa) != 0)
		return;
	encoded_size = BStream_readInt32(self);
	if (UArray_size(self->errorBa) != 0)
		return;
	if (encoded_size < 0) {
		BStream_error_(self, "negative binary array length");
		return;
	}
	data = BStream_readDataOfLength_(self, (size_t)encoded_size);
	if (encoded_size > 0 && NULL == data)
		return;
	UArray_setData_type_size_copy_(b, data, CTYPE_uint8_t,
		(size_t)encoded_size, 1);
	if (b->itemSize != 1 || b->size != (size_t)encoded_size ||
		(encoded_size > 0 && NULL == b->data))
		BStream_error_(self, "unable to allocate binary array");
}


UArray *BStream_readUArray(BStream *self)
{
	if (NULL == self || NULL == self->tmp || NULL == self->errorBa)
		return NULL;
	BStream_readUArray_(self, self->tmp);
	return UArray_size(self->errorBa) == 0 ? self->tmp : NULL;
}

const char *BStream_readCString(BStream *self)
{
	if (NULL == self || NULL == self->tmp || NULL == self->errorBa)
		return NULL;
	BStream_readUArray_(self, self->tmp);
	if (UArray_size(self->errorBa) != 0)
		return NULL;
	return (const char *)UArray_bytes(self->tmp);
}

// tagged writing --------------------------------------

void BStream_writeTag(BStream *self, unsigned int t, unsigned int b, unsigned int a)
{
	BStreamTag tag;
	unsigned char byte;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	tag.isArray = a;
	tag.type = t;
	tag.byteCount = b;
	byte = BStreamTag_asUnsignedChar(&tag);
	{
		BStreamTag decoded = BStreamTag_FromUnsignedChar(byte);
		if (decoded.isArray != tag.isArray || decoded.type != tag.type ||
			decoded.byteCount != tag.byteCount) {
			BStream_error_(self, "unsupported binary stream tag");
			return;
		}
	}
	BStream_writeUint8_(self, byte);
}


void BStream_writeTaggedUint8_(BStream *self, uint8_t v)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	oldSize = UArray_size(self->ba);
	BStream_writeTag(self, BSTREAM_UNSIGNED_INT, 1, 0);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeUint8_(self, v);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


void BStream_writeTaggedUint32_(BStream *self, uint32_t v)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	oldSize = UArray_size(self->ba);
	BStream_writeTag(self, BSTREAM_UNSIGNED_INT, 4, 0);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeUint32_(self, v);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


void BStream_writeTaggedInt32_(BStream *self, int32_t v)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	oldSize = UArray_size(self->ba);
	BStream_writeTag(self, BSTREAM_SIGNED_INT, 4, 0);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeInt32_(self, v);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


#if !defined(__SYMBIAN32__)
void BStream_writeTaggedInt64_(BStream *self, int64_t v)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	oldSize = UArray_size(self->ba);
	BStream_writeTag(self, BSTREAM_SIGNED_INT, 8, 0);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeInt64_(self, v);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


#endif

/*
#if sizeof(double) != 8
#error BStream expects doubles to be 64bit
#endif
*/

void BStream_writeTaggedDouble_(BStream *self, double v)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return;
	oldSize = UArray_size(self->ba);
	BStream_writeTag(self, BSTREAM_FLOAT, 8, 0);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeDouble_(self, v);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


void BStream_writeTaggedData_length_(BStream *self, const unsigned char *data, size_t length)
{
	size_t oldSize;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		(self->ba->itemSize != 1) || (length > 0 && NULL == data) ||
		UArray_size(self->errorBa) != 0)
		return;
	if (length > INT32_MAX) {
		BStream_error_(self, "tagged data length is out of range");
		return;
	}
	oldSize = UArray_size(self->ba);
	if (oldSize > (size_t)-1 - 6 ||
		length > (size_t)-1 - oldSize - 6 ||
		oldSize + 6 + length == (size_t)-1) {
		BStream_error_(self, "tagged data length is out of range");
		return;
	}
	BStream_writeTag(self, BSTREAM_UNSIGNED_INT, 1, 1);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeTaggedInt32_(self, (int32_t)length);
	if (UArray_size(self->errorBa) == 0)
		BStream_writeData_length_(self, data, length);
	if (UArray_size(self->errorBa) != 0)
		BStream_rollbackWrite_(self, oldSize);
}


void BStream_writeTaggedCString_(BStream *self, const char *s)
{
	if (NULL == self || NULL == s) {
		BStream_error_(self, "invalid tagged string");
		return;
	}
	BStream_writeTaggedData_length_(self, (const unsigned char *)s, strlen(s));
}


void BStream_writeTaggedUArray_(BStream *self, UArray *ba)
{
	size_t length;
	if (NULL == self || NULL == ba || ba == self->ba || ba->itemSize == 0 ||
		ba->size > (size_t)-1 / ba->itemSize ||
		(ba->size > 0 && NULL == ba->data)) {
		BStream_error_(self, "invalid tagged array");
		return;
	}
	length = UArray_sizeInBytes(ba);
	BStream_writeTaggedData_length_(self, UArray_bytes(ba), length);
}


// reading --------------------------------------

int BStream_readTag(BStream *self, unsigned int t, unsigned int b, unsigned int a)
{
	unsigned char c;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return -1;
	c = BStream_readUint8(self);
	if (UArray_size(self->errorBa) != 0)
		return -1;
	BStreamTag readTag = BStreamTag_FromUnsignedChar(c);
	BStreamTag expectedTag = BStreamTag_TagArray_type_byteCount_(a, t, b);

	if (!BStreamTag_isEqual_(&readTag, &expectedTag)) {
		BStream_error_(self, "unexpected binary stream tag");
		return -1;
	}

	return 0;
}

/*
unsigned char BStream_readTaggedByte(BStream *self)
{
	BStream_readTag(self, BSTREAM_UNSIGNED_INT, 1, 0);
	return BStream_readByte(self);
}

int BStream_readTaggedInt(BStream *self)
{
	BStream_readTag(self, BSTREAM_SIGNED_INT, 4, 0);
	return BStream_readInt32(self);
}
*/

uint8_t BStream_readTaggedUint8(BStream *self)
{
	if (NULL == self || NULL == self->errorBa ||
		BStream_readTag(self, BSTREAM_UNSIGNED_INT, 1, 0) != 0)
		return 0;
	return BStream_readUint8(self);
}


uint32_t BStream_readTaggedUint32(BStream *self)
{
	unsigned char c;
	BStreamTag t;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return 0;
	c = BStream_readByte(self);
	if (UArray_size(self->errorBa) != 0)
		return 0;
	t = BStreamTag_FromUnsignedChar(c);

	if (t.isArray == 0 && t.type == BSTREAM_UNSIGNED_INT && t.byteCount == 1)
		return (uint32_t)BStream_readUint8(self);
	if (t.isArray == 0 && t.type == BSTREAM_UNSIGNED_INT && t.byteCount == 4)
		return (uint32_t)BStream_readUint32(self);

	BStream_error_(self, "unhandled int type/size combination");
	return 0;
}

int32_t BStream_readTaggedInt32(BStream *self)
{
	unsigned char c;
	BStreamTag t;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return 0;
	c = BStream_readByte(self);
	if (UArray_size(self->errorBa) != 0)
		return 0;
	t = BStreamTag_FromUnsignedChar(c);
	if (t.isArray == 0 && t.type == BSTREAM_UNSIGNED_INT && t.byteCount == 1)
		return (int32_t)BStream_readUint8(self);
	if (t.isArray == 0 && t.type == BSTREAM_SIGNED_INT && t.byteCount == 4)
		return BStream_readInt32(self);
	BStream_error_(self, "unhandled int type/size combination");
	return 0;
}

#if !defined(__SYMBIAN32__)
int64_t BStream_readTaggedInt64(BStream *self)
{
	unsigned char c;
	BStreamTag t;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return 0;
	c = BStream_readByte(self);
	if (UArray_size(self->errorBa) != 0)
		return 0;
	t = BStreamTag_FromUnsignedChar(c);
	if (t.isArray == 0 && t.type == BSTREAM_SIGNED_INT && t.byteCount == 8)
		return BStream_readInt64(self);
	if (t.isArray == 0 && t.type == BSTREAM_SIGNED_INT && t.byteCount == 4)
		return (int64_t)BStream_readInt32(self);
	if (t.isArray == 0 && t.type == BSTREAM_UNSIGNED_INT && t.byteCount == 1)
		return (int64_t)BStream_readUint8(self);
	BStream_error_(self, "unhandled 64-bit integer tag");
	return 0;
}
#endif


intptr_t BStream_readTaggedPointer(BStream *self)
{
	unsigned char c;
	BStreamTag t;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return 0;
	c = BStream_readByte(self);
	if (UArray_size(self->errorBa) != 0)
		return 0;
	t = BStreamTag_FromUnsignedChar(c);

	if (t.isArray != 0 || t.type != BSTREAM_POINTER) {
		BStream_error_(self, "expected pointer");
		return 0;
	}

	if (t.byteCount == 1)
	{
		return (intptr_t)BStream_readUint8(self);
	}

	if (t.byteCount == 4)
	{
		return (intptr_t)BStream_readInt32(self);
	}

#if !defined(__SYMBIAN32__)
	if (t.byteCount == 8)
	{
		return (intptr_t)BStream_readInt64(self);
	}
#endif

	BStream_error_(self, "unhandled pointer size");
	return 0;
}

double BStream_readTaggedDouble(BStream *self)
{
	unsigned char c;
	BStreamTag t;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		UArray_size(self->errorBa) != 0)
		return 0.0;
	c = BStream_readByte(self);
	if (UArray_size(self->errorBa) != 0)
		return 0.0;
	t = BStreamTag_FromUnsignedChar(c);
	/*
	if (t.type == BSTREAM_FLOAT && t.byteCount == 4)
	{
		return BStream_readFloat(self);
	}
	*/
	if (t.isArray == 0 && t.type == BSTREAM_FLOAT && t.byteCount == 8)
	{
		return BStream_readDouble(self);
	}

	BStream_error_(self, "unhandled float type/size combination");
	return 0;
}

void BStream_readTaggedUArray_(BStream *self, UArray *b)
{
	int32_t encoded_size;
	unsigned char *data;
	if (NULL == self || NULL == self->ba || NULL == self->errorBa ||
		NULL == b || b == self->ba || self->ba->itemSize != 1 ||
		UArray_size(self->errorBa) != 0)
		return;
	if (BStream_readTag(self, BSTREAM_UNSIGNED_INT, 1, 1) != 0)
		return;
	encoded_size = BStream_readTaggedInt32(self);
	if (UArray_size(self->errorBa) != 0)
		return;
	if (encoded_size < 0) {
		BStream_error_(self, "negative tagged array length");
		return;
	}
	data = BStream_readDataOfLength_(self, (size_t)encoded_size);
	if (encoded_size > 0 && NULL == data)
		return;
	UArray_setData_type_size_copy_(b, data, CTYPE_uint8_t,
		(size_t)encoded_size, 1);
	if (b->itemSize != 1 || b->size != (size_t)encoded_size ||
		(encoded_size > 0 && NULL == b->data))
		BStream_error_(self, "unable to allocate tagged array");
}


UArray *BStream_readTaggedUArray(BStream *self)
{
	if (NULL == self || NULL == self->tmp || NULL == self->errorBa)
		return NULL;
	BStream_readTaggedUArray_(self, self->tmp);
	return UArray_size(self->errorBa) == 0 ? self->tmp : NULL;
}

const char *BStream_readTaggedCString(BStream *self)
{
	if (NULL == self || NULL == self->tmp || NULL == self->errorBa)
		return NULL;
	BStream_readTaggedUArray_(self, self->tmp);
	if (UArray_size(self->errorBa) != 0)
		return NULL;
	return UArray_asCString(self->tmp);
}


int BStream_atEnd(BStream *self)
{
	return NULL == self || NULL == self->ba ||
		self->index >= UArray_size(self->ba);
}

int BStream_showInt(BStream *self)
{
	unsigned char c = BStream_readUint8(self);
	BStreamTag t = BStreamTag_FromUnsignedChar(c);
	int v = 0;

	printf("%s%i ", BStreamTag_typeName(&t), t.byteCount * 8);

	if (t.byteCount < 5)
	{
		BStream_readNumber_size_(self, (unsigned char *)(&v), t.byteCount);
	}
	else
	{
		printf("ERROR: byteCount out of range\n");
		exit(-1);
	}

	printf("%i", v);
	return v;
}

void BStream_show(BStream *self)
{
	int pos = self->index;
	int v = 0;

	self->index = 0;

	while (!BStream_atEnd(self))
	{
		unsigned char c = BStream_readUint8(self);
		BStreamTag t = BStreamTag_FromUnsignedChar(c);

		/*printf("isArray:%i type:%s byteCount:%i value:", t.isArray, BStreamTag_typeName(t), t.byteCount);*/
		printf("  %s%i %s", BStreamTag_typeName(&t), t.byteCount * 8, t.isArray ? "array " : "");
		fflush(stdout);

		if (t.isArray)
		{
			printf("[");

			if (t.byteCount == 1)
			{
				int size = BStream_showInt(self);
				if (size == 0)
				{
					printf(" '']\n");
				}
				else
				{
					unsigned char *data = BStream_readDataOfLength_(self, size);
					printf(" '%s']\n", data);
				}
			}
			else
			{
				printf("ERROR: array element byteCount not 1\n");
				exit(-1);
			}
		}
		else
		{
			if (t.byteCount > 0 && t.byteCount < 5)
			{
				BStream_readNumber_size_(self, (unsigned char *)(&v), t.byteCount);
			}
			else
			{
				printf("ERROR: byteCount out of range\n");
				exit(1);
			}

			/*
			if (t.byteCount == 1)
			{
				printf("%c\n", v);
			}
			else
			{
				*/
			printf("%i\n", v);
			/*}*/
		}
	}
	self->index = pos;
}
