/*#io
UDBRecord ioDoc(
docCopyright("Steve Dekorte", 2004)
docLicense("BSD revised")
docObject("UDBRecord")    
docDescription("")
*/

#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

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

void UDBRecord_remove(UDBRecord *self)
{
    if (NULL == self)
        return;
    self->header.tag = RECORD_TAG_EMPTY;
    UDBRecord_saveHeader(self);
}

int UDBRecord_isEmpty(UDBRecord *self)
{
    return (self->header.tag == RECORD_TAG_EMPTY);
}


// save ----------------------------------------- 

int UDBRecord_saveHeader(UDBRecord *self)
{
    if (NULL == self || NULL == self->file ||
        self->pos > (PID_TYPE)LONG_MAX)
        return 0;
    JFile_setPosition_(self->file, (long)self->pos);
    return JFile_fwrite(self->file, (unsigned char *)&self->header,
                        sizeof(UDBRecordHeader), 1) == 1;
}

static int UDBRecord_rollbackSave(UDBRecord *self, long log_start,
                                  long old_file_pos, long old_max_pos)
{
    JFile *file = self->file;
    if (file->journalingOn) {
        if (file->log && log_start >= 0 &&
            fflush(file->log) == 0 &&
            ftruncate(fileno(file->log), (off_t)log_start) == 0 &&
            fseek(file->log, log_start, SEEK_SET) == 0) {
            clearerr(file->log);
            file->pos = old_file_pos;
            file->maxPos = old_max_pos;
            return 1;
        }
        if (file->log) {
            fclose(file->log);
            file->log = NULL;
        }
        return 0;
    }

    /* Record saves append at EOF, so truncation safely removes a partial row. */
    if (file->file && old_file_pos == old_max_pos &&
        ftruncate(fileno(file->file), (off_t)old_max_pos) == 0 &&
        fseek(file->file, old_file_pos, SEEK_SET) == 0) {
        clearerr(file->file);
        file->pos = old_file_pos;
        file->maxPos = old_max_pos;
        return 1;
    }
    if (file->file) {
        fclose(file->file);
        file->file = NULL;
    }
    return 0;
}

int UDBRecord_saveWithDatum_(UDBRecord *self, Datum d)
{
    UDBRecordHeader old_header;
    long old_file_pos, old_max_pos, log_start = -1;

    if (NULL == self || NULL == self->file ||
        (d.size > 0 && NULL == d.data) ||
        self->file->file == NULL || self->file->maxPos < 0 ||
        self->pos > (PID_TYPE)LONG_MAX)
        return 0;

    old_file_pos = self->file->pos;
    old_max_pos = self->file->maxPos;
    if (self->file->journalingOn) {
        if (NULL == self->file->log ||
            fseek(self->file->log, 0, SEEK_END) != 0 ||
            (log_start = ftell(self->file->log)) < 0)
            return 0;
    } else if (self->pos != (PID_TYPE)old_max_pos) {
        return 0;
    }

    old_header = self->header;
    self->header.tag = RECORD_TAG_NORMAL;
    self->header.size = d.size;
    if (!UDBRecord_saveHeader(self))
        goto failed;
    if (d.size > 0 &&
        JFile_fwrite(self->file, d.data, d.size, 1) != 1)
        goto failed;
    return 1;

failed:
    UDBRecord_rollbackSave(self, log_start, old_file_pos, old_max_pos);
    self->header = old_header;
    return 0;
}

// read ----------------------------------------- 

int UDBRecord_readHeader(UDBRecord *self)
{
    int objectsRead;
    size_t file_size;

    if (NULL == self || NULL == self->file || self->file->maxPos < 0)
        return 0;

    file_size = (size_t)self->file->maxPos;
    if (self->pos > file_size ||
        file_size - self->pos < sizeof(UDBRecordHeader)) {
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
        self->header.size > file_size - self->pos - sizeof(UDBRecordHeader)) {
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

    d.size = 0;
    d.data = NULL;
    if (NULL == self || NULL == self->file || self->file->maxPos < 0)
        return d;

    size = self->header.size;
    if (self->pos > (size_t)self->file->maxPos ||
        (size_t)self->file->maxPos - self->pos < sizeof(UDBRecordHeader) ||
        size > (size_t)self->file->maxPos - self->pos - sizeof(UDBRecordHeader))
        return d;

    ba = UDBRecord_data(self);
    if (NULL == ba)
        return d;
    JFile_fseek(self->file, (long)(self->pos + sizeof(UDBRecordHeader)), SEEK_SET);
    UArray_setSize_(ba, size);
    if (size > 0 && (NULL == UArray_bytes(ba) ||
        JFile_fread(self->file, (unsigned char *)UArray_bytes(ba), size, 1) != 1)) {
        UArray_setSize_(ba, 0);
        return d;
    }
    d.size = size;
    d.data = UArray_bytes(ba);
    return d;
}

// compact ----------------------------------------- 

void UDBRecord_moveToPos_(UDBRecord *self, PID_TYPE newPos)
{
    PID_TYPE totalSize;
    size_t file_size;
    unsigned char *data;
    UArray *ba;

    if (NULL == self || NULL == self->file || self->file->maxPos < 0 ||
        self->pos > (PID_TYPE)LONG_MAX || newPos > (PID_TYPE)LONG_MAX)
        return;

    file_size = (size_t)self->file->maxPos;
    totalSize = UDBRecord_totalSize(self);
    if (totalSize == 0 || (size_t)self->pos > file_size ||
        (size_t)totalSize > file_size - (size_t)self->pos ||
        (size_t)newPos > file_size ||
        (size_t)totalSize > file_size - (size_t)newPos)
        return;

    ba = UDBRecord_data(self);
    if (NULL == ba)
        return;
    UArray_setSize_(ba, totalSize);
    data = (unsigned char *)UArray_bytes(ba);
    if (NULL == data)
        return;

    JFile_setPosition_(self->file, (long)self->pos);
    if (JFile_fread(self->file, data, totalSize, 1) != 1) {
        UArray_setSize_(ba, 0);
        return;
    }

    JFile_setPosition_(self->file, (long)newPos);
    JFile_fwrite(self->file, data, totalSize, 1);
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


