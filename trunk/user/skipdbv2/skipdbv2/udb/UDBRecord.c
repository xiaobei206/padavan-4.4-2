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
    UDBRecord *clone = (UDBRecord *)cpalloc(self, sizeof(UDBRecord));
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
    self->file = file;
}

void UDBRecord_setPos_(UDBRecord *self, PID_TYPE pos)
{   
    self->pos = pos;
}

PID_TYPE UDBRecord_pos(UDBRecord *self)
{
    return self->pos;
}

void UDBRecord_setPosToEnd(UDBRecord *self)
{
    self->pos = JFile_setPositionToEnd(self->file);
}

void UDBRecord_pid_(UDBRecord *self, PID_TYPE pid)
{
    self->header.pid = pid;
}

PID_TYPE UDBRecord_pid(UDBRecord *self)
{
    return self->header.pid;
}

void UDBRecord_size_(UDBRecord *self, PID_TYPE size)
{
    self->header.size = size;
}

PID_TYPE UDBRecord_size(UDBRecord *self)
{
    return self->header.size;
}

int UDBRecord_isEmpty(UDBRecord *self)
{
    return (self->header.tag == RECORD_TAG_EMPTY);
}

PID_TYPE UDBRecord_totalSize(UDBRecord *self)
{
    if (NULL == self || self->header.size > (PID_TYPE)-1 - sizeof(UDBRecordHeader))
        return 0;
    return sizeof(UDBRecordHeader) + self->header.size;
}

void UDBRecord_show(UDBRecord *self)
{
    printf("Record pos: %" PID_FORMAT "\n", self->pos);
    printf("  pid: %" PID_FORMAT "\n", self->header.pid);
    printf("  size: %" PID_FORMAT "\n", self->header.size);
    printf("\n");
}
