/* Bounds-checked physical record relocation for UDB compaction. */
#include "UDB.h"
#include <stdlib.h>
#include <limits.h>

int UDBRecord_moveToPos_(UDBRecord *self, PID_TYPE newPos)
{
    PID_TYPE totalSize;
    size_t file_size;
    unsigned char *data;
    UArray *ba;
    UDBRecordHeader old_header;
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
    if (!UDBRecord_readHeader(self))
        goto failed;

    /* The physical move extent must come from the validated on-disk header. */
    file_size = (size_t)self->file->maxPos;
    totalSize = UDBRecord_totalSize(self);
    if (totalSize == 0 || (size_t)self->pos > file_size ||
        (size_t)totalSize > file_size - (size_t)self->pos ||
        (size_t)newPos > file_size ||
        (size_t)totalSize > file_size - (size_t)newPos)
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
