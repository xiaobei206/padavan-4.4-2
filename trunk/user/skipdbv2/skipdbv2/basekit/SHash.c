
//metadoc SHash copyright Steve Dekorte 2002, Marc Fauconneau 2007
//metadoc SHash license BSD revised

#define SHASH_C
#include "SHash.h"
#undef SHASH_C
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void SHash_print(SHash *self)
{
	unsigned int i, j, count[2] = {0, 0};
	size_t memory;
	if (NULL == self) {
		printf("SHash <null>\n");
		return;
	}
	memory = SHash_memorySize(self);
	printf("log2tableSize = %u\n", self->log2tableSize);
	printf("tableSize = %u\n", self->tableSize);
	printf("numKeys = %u\n", self->numKeys);
	printf("mask = %u\n", self->mask);
	printf("balance = %u\n", self->balance);
	printf("nullRecord.key = %p\n", self->nullRecord.key);
	printf("nullRecord.value = %p\n", self->nullRecord.value);
	printf("\nmemory usage : %lu bytes\n", (unsigned long)memory);
	printf("\ndensity : %f\n", memory ? (self->numKeys * sizeof(SHashRecord)) / (double)memory : 0.0);
	if (NULL == self->records || self->tableSize == 0) {
		printf("<uninitialized table>\n");
		return;
	}
	for (j = 0; j < 2; ++j) {
		for (i = 0; i < self->tableSize; ++i) {
			SHashRecord *record = SHASH_RECORDS_AT_(self, j, i);
			if (record->key) { putchar('x'); count[j]++; }
			else if (record->value) putchar('!');
			else putchar('_');
		}
		putchar('\n');
	}
	printf("balance : %u / %u [%1.3f]\n", count[0], count[1],
		(count[0] + count[1]) ? ((double)count[0] - (double)count[1]) /
		((double)count[0] + (double)count[1]) : 0.0);
}
void SHash_tableInit_(SHash* self, int log2tableSize)
{
	SHashRecord *records;
	unsigned int tableSize;
	size_t count, bytes;
	if (NULL == self || log2tableSize < 1 || log2tableSize > 20)
		return;
	tableSize = 1u << (unsigned int)log2tableSize;
	count = (size_t)tableSize * 2;
	if (count > (size_t)-1 / sizeof(SHashRecord))
		return;
	bytes = count * sizeof(SHashRecord);
	records = (SHashRecord *)io_calloc(1, bytes);
	if (NULL == records)
		return;
	self->records = records;
	self->log2tableSize = (unsigned int)log2tableSize;
	self->tableSize = tableSize;
	self->mask = tableSize - 1;
}


SHash *SHash_new(void)
{
	SHash *self = (SHash *)io_calloc(1, sizeof(SHash));
	if (NULL == self)
		return NULL;
	SHash_tableInit_(self, 1);
	if (NULL == self->records) {
		io_free(self);
		return NULL;
	}
	return self;
}


SHash *SHash_clone(SHash *self)
{
	SHash *child;
	if (NULL == self || NULL == self->records)
		return NULL;
	child = SHash_new();
	if (NULL == child)
		return NULL;
	SHash_copy_(child, self);
	if (NULL == child->records || child->tableSize != self->tableSize ||
		child->numKeys != self->numKeys ||
		child->keysEqual != self->keysEqual ||
		child->hashForKey != self->hashForKey) {
		SHash_free(child);
		return NULL;
	}
	return child;
}


void SHash_free(SHash *self)
{
	if (NULL == self)
		return;
	io_free(self->records);
	io_free(self);
}


size_t SHash_memorySize(SHash *self)
{
	if (NULL == self || self->tableSize >
		((size_t)-1 - sizeof(SHash)) / (2 * sizeof(SHashRecord)))
		return 0;
	return sizeof(SHash) + ((size_t)self->tableSize * 2 * sizeof(SHashRecord));
}


void SHash_compact(SHash *self)
{
	printf("need to implement SHash_compact\n");
}

void SHash_copy_(SHash *self, SHash *other)
{
	SHashRecord *records, *oldRecords;
	SHash copy;
	size_t bytes;
	if (NULL == self || NULL == other || self == other ||
		NULL == other->records || other->tableSize == 0 ||
		other->tableSize > (size_t)-1 / (2 * sizeof(SHashRecord)))
		return;
	bytes = (size_t)other->tableSize * 2 * sizeof(SHashRecord);
	records = (SHashRecord *)io_malloc(bytes);
	if (NULL == records)
		return;
	memcpy(records, other->records, bytes);
	copy = *other;
	copy.records = records;
	oldRecords = self->records;
	*self = copy;
	io_free(oldRecords);
}


/*	this is where our cuckoo acts :
	it kicks records out of nests until it finds an empty nest or gets tired
	returns the empty nest if found, or NULL if it is too tired
*/
SHashRecord *SHash_cuckoo_(SHash *self, SHashRecord* thisRecord)
{
	unsigned int hash;
	SHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == thisRecord || NULL == thisRecord->key ||
		NULL == self->keysEqual || NULL == self->hashForKey)
		return NULL;
	record = SHash_recordAt_(self, thisRecord->key);
	if (record != &self->nullRecord && NULL == record->key)
		return record;
	if (SHash_keysAreEqual_(self, thisRecord->key, record->key))
		return record;
	{
		int i;
		if (self->balance) {
			self->balance = 0;
			hash = SHash_hash_more(self, SHash_hash(self, thisRecord->key));
			record = SHASH_RECORDS_AT_HASH_(self, 1, hash);
			if (NULL == record->key)
				return record;
			SHashRecord_swap(record, thisRecord);
			if (SHash_keysAreEqual_(self, thisRecord->key, record->key))
				return record;
		}
		self->balance = 1;
		for (i = 0; i < SHash_maxLoops(self); i++) {
			hash = SHash_hash(self, thisRecord->key);
			record = SHASH_RECORDS_AT_HASH_(self, 0, hash);
			if (NULL == record->key)
				return record;
			SHashRecord_swap(record, thisRecord);
			if (SHash_keysAreEqual_(self, thisRecord->key, record->key))
				return record;
			hash = SHash_hash_more(self, SHash_hash(self, thisRecord->key));
			record = SHASH_RECORDS_AT_HASH_(self, 1, hash);
			if (NULL == record->key)
				return record;
			SHashRecord_swap(record, thisRecord);
			if (SHash_keysAreEqual_(self, thisRecord->key, record->key))
				return record;
		}
	}
	return NULL;
}


void SHash_grow(SHash *self)
{
	SHash saved;
	SHashRecord *oldRecords;
	unsigned int oldTableSize;
	int log2tableSize;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		self->log2tableSize >= 20 ||
		NULL == self->keysEqual || NULL == self->hashForKey)
		return;
	saved = *self;
	oldRecords = self->records;
	oldTableSize = self->tableSize;

	for (log2tableSize = (int)saved.log2tableSize + 1;
		log2tableSize <= 20; ++log2tableSize) {
		unsigned int i;
		int failed = 0;
		self->records = NULL;
		SHash_tableInit_(self, log2tableSize);
		if (NULL == self->records) {
			*self = saved;
			return;
		}
		for (i = 0; i < oldTableSize * 2; ++i) {
			SHashRecord thisRecord = oldRecords[i];
			SHashRecord *record;
			if (NULL == thisRecord.key)
				continue;
			record = SHash_cuckoo_(self, &thisRecord);
			if (NULL == record) {
				failed = 1;
				break;
			}
			*record = thisRecord;
		}
		if (!failed) {
			io_free(oldRecords);
			return;
		}
		io_free(self->records);
		self->records = NULL;
	}
	*self = saved;
}


void SHash_growWithRecord(SHash *self, SHashRecord* thisRecord)
{
	void *key, *value;
	unsigned int oldTableSize;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == thisRecord || NULL == thisRecord->key ||
		NULL == self->keysEqual || NULL == self->hashForKey)
		return;
	key = thisRecord->key;
	value = thisRecord->value;
	oldTableSize = self->tableSize;
	SHash_grow(self);
	if (self->tableSize > oldTableSize && self->records)
		SHash_at_put_(self, key, value);
}


void SHash_removeValue_(SHash *self, void *value)
{
	size_t i, count;
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return;
	count = (size_t)self->tableSize * 2;
	for (i = 0; i < count; ++i) {
		SHashRecord *record = self->records + i;
		if (record->key && record->value == value) {
			if (self->numKeys > 0)
				self->numKeys--;
			memset(record, 0, sizeof(SHashRecord));
			return;
		}
	}
}


void *SHash_firstKeyForValue_(SHash *self, void *v)
{
	size_t i, count;
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return NULL;
	count = (size_t)self->tableSize * 2;
	for (i = 0; i < count; ++i) {
		SHashRecord *record = self->records + i;
		if (record->key && record->value == v)
			return record->key;
	}
	return NULL;
}
