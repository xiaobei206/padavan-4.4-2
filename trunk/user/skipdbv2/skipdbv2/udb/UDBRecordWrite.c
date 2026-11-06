/* UDB record-header persistence, append, and removal operations. */
#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

/* Restore JFile's logical cursor bounds after a failed record mutation. */
static void UDBRecord_restoreFileState_(JFile *file, long position, long max_position)
{
    if (NULL == file)
        return;
    file->pos = position;
    file->maxPos = max_position;
}

int UDBRecord_remove(UDBRecord *self)
{
    UDBRecordHeader caller_header, stored_header;
    long old_pos, old_max;

    if (NULL == self || NULL == self->file || NULL == self->file->file ||
        self->file->maxPos < 0 || self->pos > (PID_TYPE)LONG_MAX ||
        self->pos < (PID_TYPE)sizeof(PID_TYPE) ||
        self->pos > (PID_TYPE)self->file->maxPos ||
        (PID_TYPE)self->file->maxPos - self->pos < sizeof(UDBRecordHeader))
        return 0;

    /* The public record struct may contain a stale header cache. */
    caller_header = self->header;
    old_pos = self->file->pos;
    old_max = self->file->maxPos;
    if (!UDBRecord_readHeader(self)) {
        self->header = caller_header;
        UDBRecord_restoreFileState_(self->file, old_pos, old_max);
        return 0;
    }

    /* Preserve the persisted PID and payload extent; only change the tag. */
    stored_header = self->header;
    self->header.tag = RECORD_TAG_EMPTY;
    if (!UDBRecord_saveHeader(self)) {
        self->header = stored_header;
        UDBRecord_restoreFileState_(self->file, old_pos, old_max);
        return 0;
    }
    return 1;
}

// save ----------------------------------------- 

int UDBRecord_saveHeader(UDBRecord *self)
{
    long old_pos, old_max;
    if (NULL == self || NULL == self->file ||
        self->pos > (PID_TYPE)LONG_MAX)
        return 0;
    old_pos = self->file->pos;
    old_max = self->file->maxPos;
    JFile_setPosition_(self->file, (long)self->pos);
    if (JFile_fwrite(self->file, (unsigned char *)&self->header,
                     sizeof(UDBRecordHeader), 1) == 1)
        return 1;
    UDBRecord_restoreFileState_(self->file, old_pos, old_max);
    return 0;
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
            UDBRecord_restoreFileState_(file, old_file_pos, old_max_pos);
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
        UDBRecord_restoreFileState_(file, old_file_pos, old_max_pos);
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
        d.size > (size_t)LONG_MAX - sizeof(UDBRecordHeader) ||
        self->pos > (PID_TYPE)LONG_MAX - sizeof(UDBRecordHeader) - d.size)
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
