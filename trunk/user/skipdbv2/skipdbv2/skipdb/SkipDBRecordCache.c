/* SkipDB record age-list and cache-reference maintenance. */
#include "SkipDBRecordInternal.h"
#include <stdio.h>

void SkipDBRecord_setOlderRecord_(SkipDBRecord *self, SkipDBRecord *r)
{
	if (NULL == self || r == self)
		return;
	self->olderRecord = r;
}

SkipDBRecord *SkipDBRecord_olderRecord(SkipDBRecord *self)
{
	return self ? self->olderRecord : NULL;
}

void SkipDBRecord_setYoungerRecord_(SkipDBRecord *self, SkipDBRecord *r)
{
	if (NULL == self || r == self)
		return;
	self->youngerRecord = r;
}

SkipDBRecord *SkipDBRecord_youngerRecord(SkipDBRecord *self)
{
	return self ? self->youngerRecord : NULL;
}

void SkipDBRecord_removeFromAgeList(SkipDBRecord *self)
{
	SkipDBRecord *older, *younger;
	if (NULL == self)
		return;
	older = self->olderRecord;
	younger = self->youngerRecord;
	if (older)
		SkipDBRecord_setYoungerRecord_(older, younger);
	if (younger)
		SkipDBRecord_setOlderRecord_(younger, older);
	self->olderRecord = NULL;
	self->youngerRecord = NULL;
}

void SkipDBRecord_showAgeList(SkipDBRecord *self)
{
	SkipDBRecord *r = self;

	printf("age list:\n");

	while (r)
	{
		printf("  record %p '%s'\n",
			  (void *)r, (char *)UArray_asCString(r->key));
		r = r->olderRecord;
	}
}

/* Validate pointer metadata before any cache-maintenance loop indexes it. */
static int SkipDBRecord_hasValidCachePointers_(SkipDBRecord *self)
{
	return self != NULL && self->level >= 0 &&
		self->level <= SKIPDB_MAX_LEVEL &&
		(self->level == 0 || self->pointers != NULL);
}

int SkipDBRecord_pointersAreEmpty(SkipDBRecord *self)
{
	SkipDBPointer *pointers;
	if (NULL == self)
		return 1;
	if (!SkipDBRecord_hasValidCachePointers_(self))
		return 0;
	pointers = self->pointers;

	if (pointers)
	{
		int i, max = self->level;

		for (i = 0; i < max; i ++)
		{
			SkipDBRecord *r = pointers[i].record;
			PID_TYPE pid = pointers[i].pid;

			if (r || pid) return 0;
		}
	}

	return 1;
}

void SkipDBRecord_removeReferencesToUnmarked(SkipDBRecord *self)
{
	SkipDBPointer *pointers;
	if (!SkipDBRecord_hasValidCachePointers_(self))
		return;
	pointers = self->pointers;

	if (pointers)
	{
		int i, max = self->level;

		for (i = 0; i < max; i ++)
		{
			SkipDBRecord *r = pointers[i].record;

			if (r && r->mark == 0)
			{
				pointers[i].pid = SkipDBRecord_pid(r);
				pointers[i].record = NULL;
			}
		}
	}

	if (self->previousRecord && self->previousRecord->mark == 0)
	{
		self->previousPid = SkipDBRecord_pid(self->previousRecord);
		self->previousRecord = NULL;
	}
}
