//metadoc PHash copyright Steve Dekorte 2002, Marc Fauconneau 2007
//metadoc PHash license BSD revised
/*metadoc PHash description
	PHash - Cuckoo Hash
	keys and values are references (they are not copied or freed)
	key pointers are assumed unique
*/

#ifdef PHASH_C
#define IO_IN_C_FILE
#endif
#include "Common_inline.h"
#include <limits.h>
#ifdef IO_DECLARE_INLINES

#include <stdio.h>

#define PHASH_RECORDS_AT_(self, tableIndex, index) (self->records + self->tableSize*tableIndex + index)
#define PHASH_RECORDS_AT_HASH_(self, tableIndex, hash) \
(self->records + self->tableSize*tableIndex + (hash & self->mask))

IOINLINE unsigned int PHash_count(PHash *self)
{
	return self ? self->numKeys : 0;
}


IOINLINE void PHashRecord_swap(PHashRecord* r1, PHashRecord* r2)
{
	PHashRecord t;
	if (NULL == r1 || NULL == r2 || r1 == r2)
		return;
	t = *r1;
	*r1 = *r2;
	*r2 = t;
}


#define PHashKey_value(k) k

/*
IOINLINE void *PHashKey_value(void *key)
{
	return key;
}
*/

IOINLINE unsigned int PHashKey_isEqual_(void *key1, void *key2)
{
	return key1 != NULL && key1 == key2;
}


// simple hash functions, should be enough for pointers
IOINLINE unsigned int PHash_hash(PHash *self, void *key)
{
	uintptr_t k = (uintptr_t)PHashKey_value(key);
	(void)self;
	return (unsigned int)(k ^ (k >> 4));
}


IOINLINE unsigned int PHash_hash_more(PHash *self, unsigned int hash)
{
	if (NULL == self || self->log2tableSize >= sizeof(unsigned int) * CHAR_BIT)
		return hash;
	return hash ^ (hash >> self->log2tableSize);
}


// -----------------------------------

IOINLINE void PHash_clean(PHash *self)
{
	if (NULL == self || NULL == self->records || self->tableSize == 0)
		return;
	memset(self->records, 0, sizeof(PHashRecord) * (size_t)self->tableSize * 2);
	self->numKeys = 0;
}


IOINLINE PHashRecord *PHash_recordAt_(PHash *self, void *key)
{
	unsigned int hash;
	PHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 || NULL == key)
		return self ? &self->nullRecord : NULL;
	hash = PHash_hash(self, key);
	record = PHASH_RECORDS_AT_HASH_(self, 0, hash);
	if (record->key == key)
		return record;
	hash = PHash_hash_more(self, hash);
	record = PHASH_RECORDS_AT_HASH_(self, 1, hash);
	return record->key == key ? record : &self->nullRecord;
}


IOINLINE void *PHash_at_(PHash *self, void *key)
{
	PHashRecord *record = PHash_recordAt_(self, key);
	return record ? record->value : NULL;
}


IOINLINE unsigned char PHash_at_update_(PHash *self, void *key, void *value)
{
	PHashRecord *record;
	if (NULL == self || NULL == key)
		return 0;
	record = PHash_recordAt_(self, key);
	if (NULL == record || record == &self->nullRecord || record->key != key ||
		record->value == value)
		return 0;
	record->value = value;
	return 1;
}


IOINLINE void PHash_at_put_(PHash *self, void *key, void *value)
{
	PHashRecord thisRecord;
	PHashRecord *record;
	if (NULL == self || NULL == self->records || self->tableSize == 0 || NULL == key)
		return;
	record = PHash_recordAt_(self, key);
	if (record && record != &self->nullRecord && record->key == key) {
		record->value = value;
		return;
	}
	thisRecord.key = key;
	thisRecord.value = value;
	record = PHash_cuckoo_(self, &thisRecord);
	if (NULL == record || record == &self->nullRecord) {
		PHash_growWithRecord(self, &thisRecord);
		return;
	}
	*record = thisRecord;
	self->numKeys++;
	if (self->numKeys > PHash_maxKeys(self))
		PHash_grow(self);
}


IOINLINE void PHash_removeKey_(PHash *self, void *key)
{
	PHashRecord *record;
	if (NULL == self || NULL == key)
		return;
	record = PHash_recordAt_(self, key);
	if (record && record != &self->nullRecord && record->key == key) {
		if (self->numKeys > 0)
			self->numKeys--;
		memset(record, 0, sizeof(PHashRecord));
	}
}


// --- enumeration --------------------------------------------------

#define PHASH_FOREACH(self, pkey, pvalue, code) \
{ \
	PHash *_self = (self); \
	unsigned int _i, _j; \
	void *pkey; \
	void *pvalue; \
	if (_self && _self->records && _self->tableSize) { \
	for (_j = 0; _j < 2; _j ++) \
	for (_i = 0; _i < _self->tableSize; _i ++) { \
		PHashRecord *_record = PHASH_RECORDS_AT_(_self, _j, _i); \
		if (_record->key) { pkey = _record->key; pvalue = _record->value; code; } \
	} \
	} \
}


/*
typedef BASEKIT_API void (PHashDoCallback)(void *);

IOINLINE void PHash_do_(PHash *self, PHashDoCallback *callback)
{ PHASH_FOREACH(self, k, v, (*callback)(v)); }

IOINLINE void *PHash_detect_(PHash *self, PHashDetectCallback *callback)
{ PHASH_FOREACH(self, k, v, if ((*callback)(v)) return k; ); return NULL; }

IOINLINE void PHash_doOnKeys_(PHash *self, PHashDoCallback *callback)
{ PHASH_FOREACH(self, k, v, (*callback)(k)); }

IOINLINE void PHash_doOnKeyAndValue_(PHash *self, PHashDoCallback *callback)
{ PHASH_FOREACH(self, k, v, (*callback)(k); (*callback)(v);); }
*/

#undef IO_IN_C_FILE
#endif
