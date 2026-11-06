/* Bounds-checked physical record relocation for UDB compaction. */
#include "UDB.h"
#include <stdlib.h>
#include <limits.h>

/*
 * Resolve a physical record boundary by walking validated persisted extents.
 * Callers must not treat header-shaped bytes inside a payload as a record.
 */
static int UDBRecord_headerAtBoundary_(JFile *file, PID_TYPE targetPos,
                                       UDBRecordHeader *header)
{
    UDBRecord probe = {0};
    PID_TYPE cursor = (PID_TYPE)sizeof(PID_TYPE);
    long old_pos, old_max;
    int found = 0;

    if (NULL == file || NULL == file->file || file->maxPos < 0 ||
        NULL == header || targetPos < (PID_TYPE)sizeof(PID_TYPE) ||
        targetPos > (PID_TYPE)LONG_MAX)
        return 0;

    old_pos = file->pos;
    old_max = file->maxPos;
    probe.file = file;

    while (cursor < targetPos) {
        PID_TYPE extent;

        probe.pos = cursor;
        if (!UDBRecord_readHeader(&probe))
            goto done;
        extent = UDBRecord_totalSize(&probe);
        if (extent == 0 || extent > (PID_TYPE)LONG_MAX ||
            cursor > (PID_TYPE)LONG_MAX - extent)
            goto done;
        cursor += extent;
    }
    if (cursor != targetPos)
        goto done;

    probe.pos = targetPos;
    if (!UDBRecord_readHeader(&probe))
        goto done;
    *header = probe.header;
    found = 1;

done:
    file->pos = old_pos;
    file->maxPos = old_max;
    return found;
}

int UDBRecord_moveToPos_(UDBRecord *self, PID_TYPE newPos)
{
    PID_TYPE totalSize;
    size_t file_size;
    unsigned char *data;
    UArray *ba;
    UDBRecordHeader old_header, destination_header;
    UDBRecord destination = {0};
    long old_pos, old_max;

    if (NULL == self || NULL == self->file || self->file->file == NULL ||
        self->file->maxPos < 0 || self->pos > (PID_TYPE)LONG_MAX ||
        self->pos < (PID_TYPE)sizeof(PID_TYPE) ||
        newPos > (PID_TYPE)LONG_MAX ||
        newPos < (PID_TYPE)sizeof(PID_TYPE))
        return 0;

    old_header = self->header;
    old_pos = self->file->pos;
    old_max = self->file->maxPos;
    if (!UDBRecord_headerAtBoundary_(self->file, self->pos, &self->header))
        goto failed;

    /* The physical move extent must come from the validated on-disk header. */
    file_size = (size_t)self->file->maxPos;
    totalSize = UDBRecord_totalSize(self);
    if (totalSize == 0 || (size_t)self->pos > file_size ||
        (size_t)totalSize > file_size - (size_t)self->pos ||
        (size_t)newPos > file_size ||
        (size_t)totalSize > file_size - (size_t)newPos)
        goto failed;

    if (newPos == self->pos)
        return 1;

    /*
     * Compaction may only overwrite a real free extent with enough capacity.
     * Reject live records and offsets that merely resemble header bytes.
     */
    if (!UDBRecord_headerAtBoundary_(self->file, newPos, &destination_header))
        goto failed;
    destination.header = destination_header;
    if (!UDBRecord_isEmpty(&destination) ||
        UDBRecord_totalSize(&destination) < totalSize)
        goto failed;

    ba = UDBRecord_data(self);
    if (NULL == ba)
        goto failed;
    UArray_setSize_(ba, (size_t)totalSize);
    data = (unsigned char *)UArray_bytes(ba);
    if (UArray_size(ba) != (size_t)totalSize || NULL == data)
        goto failed;

    JFile_setPosition_(self->file, (long)self->pos);
    if (JFile_fread(self->file, data, totalSize, 1) != 1) {
        UArray_setSize_(ba, 0);
        goto failed;
    }

    JFile_setPosition_(self->file, (long)newPos);
    if (JFile_fwrite(self->file, data, totalSize, 1) != 1)
        goto failed;
    return 1;

failed:
    self->header = old_header;
    self->file->pos = old_pos;
    self->file->maxPos = old_max;
    return 0;
}
