#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    UDB *db = NULL;
    UDBRecord *record = NULL;
    UDBRecord *clone = NULL;
    UDBRecord *probe = NULL;
    unsigned char payload[] = {0x41, 0x00, 0x7f, 0x80, 0xff, 0x5a};
    Datum input = {sizeof(payload), payload};
    Datum invalid = {1, NULL};
    Datum restored;
    PID_TYPE saved_pos;
    long saved_file_end;
    char path[128];
    int path_len;
    int result = EXIT_FAILURE;

    db = UDB_new();
    if (NULL == db) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }
    path_len = snprintf(path, sizeof(path), "udb_record_storage_test_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        fprintf(stderr, "unable to construct test database path\n");
        goto done;
    }

    UDB_setPath_(db, path);
    UDB_delete(db);
    UDB_open(db);
    if (!UDB_isOpen(db) || NULL == db->records || NULL == db->records->file) {
        fprintf(stderr, "database record stream did not open\n");
        goto done;
    }

    record = UDBRecord_new();
    if (NULL == record) {
        fprintf(stderr, "UDBRecord allocation failed\n");
        goto done;
    }
    UDBRecord_setJFile_(record, db->records->file);

    /* Exercise the append/write path with binary data, including NUL/high bits. */
    UDB_beginTransaction(db);
    UDBRecord_setPosToEnd(record);
    UDBRecord_pid_(record, (PID_TYPE)77);
    if (!UDB_isOpen(db) || !UDBRecord_saveWithDatum_(record, input)) {
        fprintf(stderr, "binary record append failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "record append transaction did not commit\n");
        goto done;
    }

    saved_pos = UDBRecord_pos(record);
    saved_file_end = db->records->file->maxPos;
    if (UDBRecord_totalSize(record) !=
        (PID_TYPE)(sizeof(UDBRecordHeader) + sizeof(payload)) ||
        saved_file_end < 0 || saved_pos >= (PID_TYPE)saved_file_end ||
        !UDBRecord_readHeader(record) ||
        UDBRecord_pid(record) != (PID_TYPE)77 ||
        UDBRecord_size(record) != (PID_TYPE)sizeof(payload)) {
        fprintf(stderr, "record header/size round-trip failed\n");
        goto done;
    }

    restored = UDBRecord_readDatum(record);
    if (restored.data == NULL || restored.size != sizeof(payload) ||
        memcmp(restored.data, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "binary payload round-trip failed\n");
        goto done;
    }

    /* A clone owns a distinct payload buffer and safely rejects NULL. */
    if (UDBRecord_clone(NULL) != NULL) {
        fprintf(stderr, "UDBRecord clone accepted a NULL source\n");
        goto done;
    }
    clone = UDBRecord_clone(record);
    if (NULL == clone) {
        fprintf(stderr, "UDBRecord clone allocation failed\n");
        goto done;
    }
    if (clone == record) {
        clone = NULL;
        fprintf(stderr, "UDBRecord clone returned the source object\n");
        goto done;
    }
    if (clone->data == record->data) {
        /* Keep the regression failure from double-freeing a shallow clone. */
        clone->data = NULL;
        UDBRecord_free(clone);
        clone = NULL;
        fprintf(stderr, "UDBRecord clone aliased the source payload\n");
        goto done;
    }
    if (clone->file != record->file || clone->pos != record->pos ||
        clone->header.tag != record->header.tag ||
        clone->header.pid != record->header.pid ||
        clone->header.size != record->header.size ||
        NULL == clone->data || clone->data->itemSize != 1 ||
        clone->data->size != sizeof(payload) ||
        NULL == UArray_bytes(clone->data) ||
        memcmp(UArray_bytes(clone->data), payload, sizeof(payload)) != 0) {
        fprintf(stderr, "UDBRecord clone did not preserve metadata and payload\n");
        goto done;
    }
    UDBRecord_free(clone);
    clone = NULL;
    restored = UDBRecord_readDatum(record);
    if (restored.data == NULL || restored.size != sizeof(payload) ||
        memcmp(restored.data, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "freeing a record clone damaged the source payload\n");
        goto done;
    }

    /* Invalid inputs and out-of-file moves must fail without changing bytes. */
    if (UDBRecord_saveWithDatum_(record, invalid) ||
        UDBRecord_moveToPos_(record, (PID_TYPE)saved_file_end)) {
        fprintf(stderr, "invalid write or out-of-range move was accepted\n");
        goto done;
    }
    restored = UDBRecord_readDatum(record);
    if (restored.data == NULL || restored.size != sizeof(payload) ||
        memcmp(restored.data, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "rejected operation damaged the record payload\n");
        goto done;
    }

    /* A partial trailing header is rejected before any header read is attempted. */
    probe = UDBRecord_new();
    if (NULL == probe) {
        fprintf(stderr, "UDBRecord probe allocation failed\n");
        goto done;
    }
    UDBRecord_setJFile_(probe, db->records->file);
    UDBRecord_setPos_(probe, (PID_TYPE)(saved_file_end - 1));
    if (UDBRecord_readHeader(probe) || !UDBRecord_isEmpty(probe)) {
        fprintf(stderr, "truncated record header was accepted\n");
        goto done;
    }

    /* Refuse to remove a truncated trailing record header. */
    if (UDBRecord_remove(probe) ||
        db->records->file->maxPos != saved_file_end) {
        fprintf(stderr, "removal accepted a truncated record position\n");
        goto done;
    }

    /* Removal persists only the EMPTY tag and remains readable as a tombstone. */
    UDB_beginTransaction(db);
    if (!UDBRecord_remove(record)) {
        fprintf(stderr, "record tombstone write failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !UDBRecord_readHeader(record) ||
        !UDBRecord_isEmpty(record)) {
        fprintf(stderr, "record tombstone did not survive commit\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("udb record storage test passed");

done:
    if (clone) UDBRecord_free(clone);
    if (probe) UDBRecord_free(probe);
    if (record) UDBRecord_free(record);
    UDB_delete(db);
    UDB_free(db);
    return result;
}
