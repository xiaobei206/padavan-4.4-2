//metadoc SHash copyright Steve Dekorte 2002, Marc Fauconneau 2007
//metadoc SHash license BSD revised
/*metadoc SHash description
	SHash - Cuckoo Hash
	keys and values are references (they are not copied or freed)
	key pointers are assumed unique
*/

#ifdef SHASH_C
#define IO_IN_C_FILE
#endif
#include "Common_inline.h"
#include <limits.h>
#ifdef IO_DECLARE_INLINES

#include <stdio.h>

#define SHASH_RECORDS_AT_(self, tableIndex, index) (self->records + self->tableSize*tableIndex + index)
#define SHASH_RECORDS_AT_HASH_(self, tableIndex, hash) \
(self->records + self->tableSize*tableIndex + (hash & self->mask))

IOINLINE unsigned int SHash_count(SHash *self)
{
	return self ? self->numKeys : 0;
}


IOINLINE void SHashRecord_swap(SHashRecord* r1, SHashRecord* r2)
{
	SHashRecord t;
	if (NULL == r1 || NULL == r2 || r1 == r2)
		return;
	t = *r1;
	*r1 = *r2;
	*r2 = t;
}


IOINLINE unsigned int SHash_keysAreEqual_(SHash *self, void *key1, void *key2)
{
	return self && self->keysEqual && key1 && key2
		? (unsigned int)(self->keysEqual(key1, key2) != 0) : 0;
}


// simple hash functions, should be enough for pointers

IOINLINE unsigned int SHash_hash(SHash *self, void *key)
{
	uintptr_t k;
	if (NULL == self || NULL == self->hashForKey || NULL == key)
		return 0;
	k = (uintptr_t)self->hashForKey(key);
	return (unsigned int)(k ^ (k >> 4));
}


IOINLINE unsigned int SHash_hash_more(SHash *self, unsigned int hash)
{
	if (NULL == self || self->log2tableSize >= sizeof(unsigned int) * CHAR_BIT)
		return hash;
	return hash ^ (hash >> self->log2tableSize);
}


// -----------------------------------

IOINLINE void SHash_clean(SHash *self)
{
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return;
	memset(self->records, 0, sizeof(SHashRecord) * (size_t)self->tableSize * 2);
	self->numKeys = 0;
}


IOINLINE SHashRecord *SHash_recordAt_(SHash *self, void *key)
{
	unsigned int hash;
	SHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == key || NULL == self->keysEqual || NULL == self->hashForKey)
		return self ? &self->nullRecord : NULL;
	hash = SHash_hash(self, key);
	record = SHASH_RECORDS_AT_HASH_(self, 0, hash);
	if (SHash_keysAreEqual_(self, key, record->key))
		return record;
	hash = SHash_hash_more(self, hash);
	record = SHASH_RECORDS_AT_HASH_(self, 1, hash);
	return SHash_keysAreEqual_(self, key, record->key) ? record : &self->nullRecord;
}


IOINLINE void *SHash_at_(SHash *self, void *key)
{
	SHashRecord *record = SHash_recordAt_(self, key);
	return record ? record->value : NULL;
}


IOINLINE int SHashKey_hasKey_(SHash *self, void *key)
{
	SHashRecord *record;
	if (NULL == self || NULL == key)
		return 0;
	record = SHash_recordAt_(self, key);
	return record && record != &self->nullRecord && record->key &&
		SHash_keysAreEqual_(self, key, record->key);
}


IOINLINE void SHash_at_put_(SHash *self, void *key, void *value)
{
	SHashRecord thisRecord;
	SHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 ||
		NULL == key || NULL == self->keysEqual || NULL == self->hashForKey)
		return;
	record = SHash_recordAt_(self, key);
	if (record && record != &self->nullRecord &&
		SHash_keysAreEqual_(self, key, record->key)) {
		record->value = value;
		return;
	}
	thisRecord.key = key;
	thisRecord.value = value;
	record = SHash_cuckoo_(self, &thisRecord);
	if (NULL == record || record == &self->nullRecord) {
		SHash_growWithRecord(self, &thisRecord);
		return;
	}
	*record = thisRecord;
	self->numKeys++;
	if (self->numKeys > SHash_maxKeys(self))
		SHash_grow(self);
}


IOINLINE void SHash_removeKey_(SHash *self, void *key)
{
	SHashRecord *record;
	if (NULL == self || NULL == key)
		return;
	record = SHash_recordAt_(self, key);
	if (record && record != &self->nullRecord && record->key &&
		SHash_keysAreEqual_(self, record->key, key)) {
		if (self->numKeys > 0)
			self->numKeys--;
		memset(record, 0, sizeof(SHashRecord));
	}
}


// --- enumeration --------------------------------------------------

#define SHASH_FOREACH(self, pkey, pvalue, code) \
{ \
	SHash *_self = (self); \
	unsigned int _i, _j; \
	void *pkey; \
	void *pvalue; \
	if (_self && _self->records && _self->tableSize) { \
	for (_j = 0; _j < 2; _j ++) \
	for (_i = 0; _i < _self->tableSize; _i ++) { \
		SHashRecord *_record = SHASH_RECORDS_AT_(_self, _j, _i); \
		if (_record->key) { pkey = _record->key; pvalue = _record->value; code; } \
	} \
	} \
}


/*
typedef BASEKIT_API void (SHashDoCallback)(void *);
IOINLINE void SHash_do_(SHash *self, SHashDoCallback *callback)
{ SHASH_FOREACH(self, k, v, (*callback)(v)); }

IOINLINE void SHash_doOnKeys_(SHash *self, SHashDoCallback *callback)
{ SHASH_FOREACH(self, k, v, (*callback)(k)); }

IOINLINE void SHash_doOnKeyAndValue_(SHash *self, SHashDoCallback *callback)
{ SHASH_FOREACH(self, k, v, (*callback)(k); (*callback)(v);); }
*/

#undef IO_IN_C_FILE
#endif
