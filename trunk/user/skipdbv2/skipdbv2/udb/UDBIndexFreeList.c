/* PID reservation and reusable-hole pool. */
#include "UDBIndexInternal.h"
#include <stdio.h>

static int UDBIndex_pushAndVerify_(List *list, void *value)
{
	size_t old_size;
	if (NULL == list || list->size == (size_t)-1)
		return 0;
	old_size = list->size;
	List_push_(list, value);
	return list->size == old_size + 1;
}

int UDBIndex_hasReservation_(UDBIndex *self, PID_TYPE pid)
{
	size_t i;

	if (!UDBIndex_isValid_(self) || pid == 0 || pid > self->maxPid)
		return 0;
	for (i = 0; i < self->reserved->size; ++i) {
		if (self->reserved->items[i] == (void *)pid)
			return 1;
	}
	return 0;
}

int UDBIndex_reservePid_(UDBIndex *self, PID_TYPE pid)
{
	if (!UDBIndex_isValid_(self) || pid == 0 ||
		UDBIndex_hasReservation_(self, pid))
		return 0;
	return UDBIndex_pushAndVerify_(self->reserved, (void *)pid);
}

void UDBIndex_unreservePid_(UDBIndex *self, PID_TYPE pid)
{
	if (UDBIndex_isValid_(self))
		(void)List_remove_(self->reserved, (void *)pid);
}

void UDBIndex_removeHole_(UDBIndex *self, PID_TYPE pid)
{
	if (!UDBIndex_isValid_(self))
		return;
	while (List_removeFirst_(self->holes, (void *)pid)) {
		/* Normalize the free-PID stack before changing ownership. */
	}
}

/* Add exactly one reusable PID to the free stack, checking allocation. */
static int UDBIndex_pushHole_(UDBIndex *self, PID_TYPE pid)
{
	if (!UDBIndex_isValid_(self) || pid == 0)
		return 0;
	UDBIndex_removeHole_(self, pid);
	return UDBIndex_pushAndVerify_(self->holes, (void *)pid);
}

int UDBIndex_releasePid_(UDBIndex *self, PID_TYPE pid)
{
	if (!UDBIndex_isValid_(self) || pid == 0)
		return 0;
	UDBIndex_unreservePid_(self, pid);
	return UDBIndex_pushHole_(self, pid);
}

void UDBIndex_forgetPid_(UDBIndex *self, PID_TYPE pid)
{
	UDBIndex_unreservePid_(self, pid);
	UDBIndex_removeHole_(self, pid);
}

int UDBIndex_prepareCommit(UDBIndex *self)
{
	size_t i;

	if (!UDBIndex_hasStreams_(self))
		return 0;

	/* Retire unwritten reservations before UDB arms the durable marker. */
	for (i = 0; i < self->reserved->size; ++i) {
		PID_TYPE pid = (PID_TYPE)self->reserved->items[i];

		if (pid == 0)
			continue;
		if (!UDBIndex_pushHole_(self, pid))
			return 0;
	}
	List_removeAll(self->reserved);
	return 1;
}

int UDBIndex_findHoles(UDBIndex *self)
{
	PID_TYPE pid;
	long pid_count;

	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file)
		return 0;
	pid_count = UDBIndex_pidCount(self);
	if (pid_count <= 0)
		return 0;
	pid = (PID_TYPE)pid_count;
	List_removeAll(self->holes);

	/* PID 0 is reserved as the null/sentinel record, not a reusable hole. */
	while (pid > 1) {
		--pid;
		if (!UDBIndex_posForPid_(self, pid)) {
			if (ferror(self->file->file))
				return 0;
			if (!UDBIndex_pushAndVerify_(self->holes, (void *)pid))
				return 0;
		}
	}
	return 1;
}


PID_TYPE UDBIndex_nextPid(UDBIndex *self)
{
	PID_TYPE hole;
	long end_pos;

	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file)
		return 0;
	hole = (PID_TYPE)List_top(self->holes);
	if (hole)
		return hole;

	end_pos = JFile_setPositionToEnd(self->file);
	if (end_pos < 0 ||
		end_pos % (long)sizeof(UDBIndexEntry) != 0)
		return 0;
	return (PID_TYPE)(end_pos / (long)sizeof(UDBIndexEntry));
}


PID_TYPE UDBIndex_allocPid(UDBIndex *self)
{
	PID_TYPE hole, pid, max;
	size_t offset;
	long end_pos;

	if (!UDBIndex_hasFile_(self))
		return 0;

	hole = (PID_TYPE)List_top(self->holes);
	if (hole) {
		if (!UDBIndex_offsetForPid_(hole, &offset) ||
			!UDBIndex_reservePid_(self, hole))
			return 0;
		UDBIndex_removeHole_(self, hole);
		return hole;
	}

	/* Check the signed seek result before converting it to an unsigned PID. */
	end_pos = JFile_setPositionToEnd(self->file);
	if (end_pos < 0 ||
		end_pos % (long)sizeof(UDBIndexEntry) != 0)
		return 0;
	max = (PID_TYPE)(end_pos / (long)sizeof(UDBIndexEntry));
	if (max <= self->maxPid) {
		if (self->maxPid == (PID_TYPE)-1)
			return 0;
		pid = self->maxPid + 1;
	} else {
		pid = max;
	}
	if (!UDBIndex_offsetForPid_(pid, &offset) ||
		!UDBIndex_reservePid_(self, pid))
		return 0;
	self->maxPid = pid;
	return pid;
}

int UDBIndex_isReservedPid_(UDBIndex *self, PID_TYPE pid)
{
	if (!UDBIndex_hasFile_(self) || !UDBIndex_hasReservation_(self, pid) ||
		UDBIndex_posForPid_(self, pid) != 0 || ferror(self->file->file))
		return 0;
	return 1;
}
