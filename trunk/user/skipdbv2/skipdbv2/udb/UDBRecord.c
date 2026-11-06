/* UDB record identity, in-memory ownership, and metadata accessors. */
#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>

UDBRecord *UDBRecord_new(void)
{
    UDBRecord *self = (UDBRecord *)calloc(1, sizeof(UDBRecord));
    return self;
}

UDBRecord *UDBRecord_clone(UDBRecord *self)
{
    UDBRecord *clone;

    if (NULL == self)
        return NULL;
    clone = UDBRecord_new();
    if (NULL == clone)
        return NULL;

    clone->file = self->file;
    clone->pos = self->pos;
    clone->header = self->header;
    if (self->data) {
        clone->data = UArray_clone(self->data);
        if (NULL == clone->data) {
            UDBRecord_free(clone);
            return NULL;
        }
    }
    return clone;
}

void UDBRecord_free(UDBRecord *self)
{
    if (NULL == self)
        return;
    if (self->data) UArray_free(self->data);
    free(self);
}

UArray *UDBRecord_data(UDBRecord *self)
{
    if (NULL == self)
        return NULL;
    if (!self->data)
        self->data = UArray_new();
    return self->data;
}

void UDBRecord_setJFile_(UDBRecord *self, JFile *file)
{
    if (self)
        self->file = file;
}

void UDBRecord_setPos_(UDBRecord *self, PID_TYPE pos)
{
    if (self)
        self->pos = pos;
}

PID_TYPE UDBRecord_pos(UDBRecord *self)
{
    return self ? self->pos : 0;
}

void UDBRecord_setPosToEnd(UDBRecord *self)
{
    long pos;

    if (NULL == self || NULL == self->file)
        return;
    pos = JFile_setPositionToEnd(self->file);
    if (pos >= 0)
        self->pos = (PID_TYPE)pos;
}

void UDBRecord_pid_(UDBRecord *self, PID_TYPE pid)
{
    if (self)
        self->header.pid = pid;
}

PID_TYPE UDBRecord_pid(UDBRecord *self)
{
    return self ? self->header.pid : 0;
}

void UDBRecord_size_(UDBRecord *self, PID_TYPE size)
{
    if (self)
        self->header.size = size;
}

PID_TYPE UDBRecord_size(UDBRecord *self)
{
    return self ? self->header.size : 0;
}

int UDBRecord_isEmpty(UDBRecord *self)
{
    return self != NULL && self->header.tag == RECORD_TAG_EMPTY;
}

PID_TYPE UDBRecord_totalSize(UDBRecord *self)
{
    if (NULL == self || self->header.size > (PID_TYPE)-1 - sizeof(UDBRecordHeader))
        return 0;
    return sizeof(UDBRecordHeader) + self->header.size;
}

void UDBRecord_show(UDBRecord *self)
{
    if (NULL == self)
        return;
    printf("Record pos: %" PID_FORMAT "\n", self->pos);
    printf("  pid: %" PID_FORMAT "\n", self->header.pid);
    printf("  size: %" PID_FORMAT "\n", self->header.size);
    printf("\n");
}
