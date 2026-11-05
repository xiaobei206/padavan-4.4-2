/* Bounded UDB record-header validation and payload reads. */
#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Shared extent check for a fixed header plus its declared payload. */
static int UDBRecord_payloadFitsFile_(UDBRecord *self, size_t file_size, PID_TYPE payload_size)
{
    if (NULL == self || self->pos > file_size)
        return 0;
    if (file_size - self->pos < sizeof(UDBRecordHeader))
        return 0;
    return payload_size <= file_size - self->pos - sizeof(UDBRecordHeader);
}

int UDBRecord_readHeader(UDBRecord *self)
{
    int objectsRead;
    size_t file_size;

    if (NULL == self || NULL == self->file || self->file->maxPos < 0)
        return 0;

    file_size = (size_t)self->file->maxPos;
    if (!UDBRecord_payloadFitsFile_(self, file_size, 0)) {
        self->header.tag = RECORD_TAG_EMPTY;
        self->header.pid = 0;
        self->header.size = 0;
        return 0;
    }

    JFile_setPosition_(self->file, (long)self->pos);
    objectsRead = JFile_fread(self->file,
                              (unsigned char *)&self->header,
                              sizeof(UDBRecordHeader), 1);

    if (objectsRead != 1 ||
        (self->header.tag != RECORD_TAG_NORMAL &&
         self->header.tag != RECORD_TAG_EMPTY) ||
        self->header.pid == 0 ||
        !UDBRecord_payloadFitsFile_(self, file_size, self->header.size)) {
        self->header.tag = RECORD_TAG_EMPTY;
        self->header.pid = 0;
        self->header.size = 0;
        return 0;
    }

    return 1;
}

Datum UDBRecord_readDatum(UDBRecord *self)
{
    UArray *ba;
    Datum d;
    PID_TYPE size;
    long old_pos, old_max;

    d.size = 0;
    d.data = NULL;
    if (NULL == self || NULL == self->file || self->file->maxPos < 0)
        return d;

    size = self->header.size;
    if (!UDBRecord_payloadFitsFile_(self, (size_t)self->file->maxPos, size))
        return d;

    ba = UDBRecord_data(self);
    if (NULL == ba)
        return d;
    old_pos = self->file->pos;
    old_max = self->file->maxPos;
    if (UArray_size(ba) != size)
        UArray_setSize_(ba, (size_t)size);
    if (UArray_size(ba) != size ||
        (size > 0 && NULL == UArray_bytes(ba)) ||
        self->pos > (PID_TYPE)LONG_MAX - sizeof(UDBRecordHeader)) {
        self->file->pos = old_pos;
        self->file->maxPos = old_max;
        return d;
    }
    JFile_fseek(self->file, (long)(self->pos + sizeof(UDBRecordHeader)), SEEK_SET);
    if (self->file->pos != (long)(self->pos + sizeof(UDBRecordHeader)) ||
        (size > 0 && JFile_fread(self->file,
            (unsigned char *)UArray_bytes(ba), (size_t)size, 1) != 1)) {
        UArray_setSize_(ba, 0);
        self->file->pos = old_pos;
        self->file->maxPos = old_max;
        return d;
    }
    d.size = size;
    d.data = (unsigned char *)UArray_bytes(ba);
    self->file->pos = old_pos;
    self->file->maxPos = old_max;
    return d;
}
