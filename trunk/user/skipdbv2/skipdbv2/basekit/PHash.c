
//metadoc PHash copyright Steve Dekorte 2002, Marc Fauconneau 2007
//metadoc PHash license BSD revised

#define PHASH_C
#include "PHash.h"
#undef PHASH_C
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void PHash_print(PHash *self)
{
	unsigned int i, j, count[2] = {0, 0};
	size_t memory;
	if (NULL == self) {
		printf("PHash <null>\n");
		return;
	}
	memory = PHash_memorySize(self);
	printf("log2tableSize = %u\n", self->log2tableSize);
	printf("tableSize = %u\n", self->tableSize);
	printf("numKeys = %u\n", self->numKeys);
	printf("mask = %u\n", self->mask);
	printf("balance = %u\n", self->balance);
	printf("nullRecord.key = %p\n", self->nullRecord.key);
	printf("nullRecord.value = %p\n", self->nullRecord.value);
	printf("\nmemory usage : %lu bytes\n", (unsigned long)memory);
	printf("\ndensity : %f\n", memory ? (self->numKeys * sizeof(PHashRecord)) / (double)memory : 0.0);
	if (NULL == self->records || self->tableSize == 0) {
		printf("<uninitialized table>\n");
		return;
	}
	for (j = 0; j < 2; ++j) {
		for (i = 0; i < self->tableSize; ++i) {
			PHashRecord *record = PHASH_RECORDS_AT_(self, j, i);
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
void PHash_tableInit_(PHash* self, int log2tableSize)
{
	PHashRecord *records;
	unsigned int tableSize;
	size_t count, bytes;
	if (NULL == self || log2tableSize < 1 || log2tableSize > 20)
		return;
	tableSize = 1u << (unsigned int)log2tableSize;
	count = (size_t)tableSize * 2;
	if (count > (size_t)-1 / sizeof(PHashRecord))
		return;
	bytes = count * sizeof(PHashRecord);
	records = (PHashRecord *)io_calloc(1, bytes);
	if (NULL == records)
		return;
	self->records = records;
	self->log2tableSize = (unsigned int)log2tableSize;
	self->tableSize = tableSize;
	self->mask = tableSize - 1;
}


PHash *PHash_new(void)
{
	PHash *self = (PHash *)io_calloc(1, sizeof(PHash));
	if (NULL == self)
		return NULL;
	PHash_tableInit_(self, 1);
	if (NULL == self->records) {
		io_free(self);
		return NULL;
	}
	return self;
}


PHash *PHash_clone(PHash *self)
{
	PHash *child;
	if (NULL == self || NULL == self->records)
		return NULL;
	child = PHash_new();
	if (NULL == child)
		return NULL;
	PHash_copy_(child, self);
	if (NULL == child->records || child->tableSize != self->tableSize ||
		child->numKeys != self->numKeys) {
		PHash_free(child);
		return NULL;
	}
	return child;
}


void PHash_free(PHash *self)
{
	if (NULL == self)
		return;
	io_free(self->records);
	io_free(self);
}


size_t PHash_memorySize(PHash *self)
{
	if (NULL == self || self->tableSize >
		((size_t)-1 - sizeof(PHash)) / (2 * sizeof(PHashRecord)))
		return 0;
	return sizeof(PHash) + ((size_t)self->tableSize * 2 * sizeof(PHashRecord));
}


void PHash_compact(PHash *self)
{
	PHashRecord *oldRecords;
	unsigned int oldTableSize, targetLog2;
	int log2tableSize;

	if (NULL == self || NULL == self->records ||
		self->log2tableSize < 1 || self->log2tableSize > 20 ||
		self->tableSize != (1u << self->log2tableSize) ||
		self->mask != self->tableSize - 1 ||
		self->numKeys > self->tableSize ||
		self->log2tableSize <= 1 || self->numKeys >= self->tableSize)
		return;

	targetLog2 = 1;
	while (targetLog2 < self->log2tableSize &&
		(1u << targetLog2) < self->numKeys)
		targetLog2++;
	if (targetLog2 >= self->log2tableSize)
		return;

	oldRecords = self->records;
	oldTableSize = self->tableSize;
	for (log2tableSize = (int)targetLog2;
		log2tableSize < (int)self->log2tableSize; ++log2tableSize) {
		PHash candidate = *self;
		size_t i, recordCount = (size_t)oldTableSize * 2;
		int failed = 0;

		candidate.records = NULL;
		candidate.numKeys = 0;
		candidate.balance = 0;
		PHash_tableInit_(&candidate, log2tableSize);
		if (NULL == candidate.records)
			return;

		for (i = 0; i < recordCount; ++i) {
			PHashRecord thisRecord = oldRecords[i];
			PHashRecord *record;
			if (NULL == thisRecord.key)
				continue;
			record = PHash_cuckoo_(&candidate, &thisRecord);
			if (NULL == record || record == &candidate.nullRecord) {
				failed = 1;
				break;
			}
			*record = thisRecord;
			candidate.numKeys++;
		}

		if (!failed && candidate.numKeys == self->numKeys) {
			*self = candidate;
			io_free(oldRecords);
			return;
		}
		io_free(candidate.records);
	}
}

void PHash_copy_(PHash *self, PHash *other)
{
	PHashRecord *records, *oldRecords;
	PHash copy;
	size_t bytes;
	if (NULL == self || NULL == other || self == other ||
		NULL == other->records || other->tableSize == 0 ||
		other->tableSize > (size_t)-1 / (2 * sizeof(PHashRecord)))
		return;
	bytes = (size_t)other->tableSize * 2 * sizeof(PHashRecord);
	records = (PHashRecord *)io_malloc(bytes);
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
PHashRecord *PHash_cuckoo_(PHash *self, PHashRecord* thisRecord)
{
	unsigned int hash;
	PHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == thisRecord || NULL == thisRecord->key)
		return NULL;
	record = PHash_recordAt_(self, thisRecord->key);
	if (record != &self->nullRecord && NULL == record->key)
		return record;
	if (PHashKey_isEqual_(thisRecord->key, record->key))
		return record;
	{
		int i;
		if (self->balance) {
			self->balance = 0;
			hash = PHash_hash_more(self, PHash_hash(self, thisRecord->key));
			record = PHASH_RECORDS_AT_HASH_(self, 1, hash);
			if (NULL == record->key)
				return record;
			PHashRecord_swap(record, thisRecord);
			if (PHashKey_isEqual_(thisRecord->key, record->key))
				return record;
		}
		self->balance = 1;
		for (i = 0; i < PHash_maxLoops(self); i++) {
			hash = PHash_hash(self, thisRecord->key);
			record = PHASH_RECORDS_AT_HASH_(self, 0, hash);
			if (NULL == record->key)
				return record;
			PHashRecord_swap(record, thisRecord);
			if (PHashKey_isEqual_(thisRecord->key, record->key))
				return record;
			hash = PHash_hash_more(self, PHash_hash(self, thisRecord->key));
			record = PHASH_RECORDS_AT_HASH_(self, 1, hash);
			if (NULL == record->key)
				return record;
			PHashRecord_swap(record, thisRecord);
			if (PHashKey_isEqual_(thisRecord->key, record->key))
				return record;
		}
	}
	return NULL;
}


void PHash_grow(PHash *self)
{
	PHash saved;
	PHashRecord *oldRecords;
	unsigned int oldTableSize;
	int log2tableSize;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		self->log2tableSize >= 20)
		return;
	saved = *self;
	oldRecords = self->records;
	oldTableSize = self->tableSize;

	for (log2tableSize = (int)saved.log2tableSize + 1;
		log2tableSize <= 20; ++log2tableSize) {
		unsigned int i;
		int failed = 0;
		self->records = NULL;
		PHash_tableInit_(self, log2tableSize);
		if (NULL == self->records) {
			*self = saved;
			return;
		}
		for (i = 0; i < oldTableSize * 2; ++i) {
			PHashRecord thisRecord = oldRecords[i];
			PHashRecord *record;
			if (NULL == thisRecord.key)
				continue;
			record = PHash_cuckoo_(self, &thisRecord);
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


void PHash_growWithRecord(PHash *self, PHashRecord* thisRecord)
{
	void *key, *value;
	unsigned int oldTableSize;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == thisRecord || NULL == thisRecord->key)
		return;
	key = thisRecord->key;
	value = thisRecord->value;
	oldTableSize = self->tableSize;
	PHash_grow(self);
	if (self->tableSize > oldTableSize && self->records)
		PHash_at_put_(self, key, value);
}


void PHash_removeValue_(PHash *self, void *value)
{
	size_t i, count;
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return;
	count = (size_t)self->tableSize * 2;
	for (i = 0; i < count; ++i) {
		PHashRecord *record = self->records + i;
		if (record->key && record->value == value) {
			if (self->numKeys > 0)
				self->numKeys--;
			memset(record, 0, sizeof(PHashRecord));
			return;
		}
	}
}


void *PHash_firstKeyForValue_(PHash *self, void *v)
{
	size_t i, count;
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return NULL;
	count = (size_t)self->tableSize * 2;
	for (i = 0; i < count; ++i) {
		PHashRecord *record = self->records + i;
		if (record->key && record->value == v)
			return record->key;
	}
	return NULL;
}
