#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    UDB *db = NULL;
    UDB *foreign_db = NULL;
    UDBRecord *record = NULL;
    UDBRecord *clone = NULL;
    UDBRecord *probe = NULL;
    UDBRecord *foreign_record = NULL;
    unsigned char payload[] = {0x41, 0x00, 0x7f, 0x80, 0xff, 0x5a};
    Datum input = {sizeof(payload), payload};
    Datum invalid = {1, NULL};
    Datum restored;
    PID_TYPE saved_pos;
    long saved_file_end;
    char path[128];
    char foreign_path[128];
    int path_len, foreign_path_len;
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
    /* Public metadata accessors must be safe for absent records/files. */
    {
        UDBRecord null_file_record;

        memset(&null_file_record, 0, sizeof(null_file_record));
        null_file_record.pos = (PID_TYPE)23;
        UDBRecord_setJFile_(NULL, NULL);
        UDBRecord_setPos_(NULL, (PID_TYPE)99);
        UDBRecord_setPosToEnd(NULL);
        UDBRecord_pid_(NULL, (PID_TYPE)99);
        UDBRecord_size_(NULL, (PID_TYPE)99);
        UDBRecord_show(NULL);
        UDBRecord_setPosToEnd(&null_file_record);
        if (UDBRecord_pos(NULL) != 0 || UDBRecord_pid(NULL) != 0 ||
            UDBRecord_size(NULL) != 0 || UDBRecord_isEmpty(NULL) ||
            UDBRecord_totalSize(NULL) != 0 ||
            null_file_record.pos != (PID_TYPE)23) {
            fprintf(stderr, "record metadata accessors were not NULL-safe\n");
            goto done;
        }
    }
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

    /* A record header writer must never overwrite the offset-zero metadata. */
    {
        UDBRecord metadata_probe;
        size_t journal_before, journal_after;
        int save_accepted;

        memset(&metadata_probe, 0, sizeof(metadata_probe));
        UDBRecord_setJFile_(&metadata_probe, db->records->file);
        UDBRecord_setPos_(&metadata_probe, 0);
        metadata_probe.header.tag = RECORD_TAG_NORMAL;
        metadata_probe.header.pid = (PID_TYPE)77;
        metadata_probe.header.size = (PID_TYPE)sizeof(payload);
        journal_before = JFile_logSize(db->records->file);
        save_accepted = UDBRecord_saveHeader(&metadata_probe);
        journal_after = JFile_logSize(db->records->file);
        if (save_accepted || journal_after != journal_before) {
            (void)JFile_clipLog(db->records->file);
            fprintf(stderr, "record header writer accepted reserved metadata offset\n");
            goto done;
        }
    }

    /* Payload reads must not trust a stale cached size or PID. */
    record->header.pid = 0;
    record->header.size = 1;
    restored = UDBRecord_readDatum(record);
    if (restored.data == NULL || restored.size != sizeof(payload) ||
        UDBRecord_pid(record) != 77 ||
        UDBRecord_size(record) != sizeof(payload) ||
        memcmp(restored.data, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "record payload read trusted stale header size\n");
        goto done;
    }

    /*
     * Advancing a sequential cursor must derive its offset from the persisted
     * header, not the public record object's possibly stale cache.
     */
    {
        UDBRecord *cursor = UDBRecords_recordAtPos_(db->records, saved_pos);

        if (NULL == cursor) {
            fprintf(stderr, "unable to position record cursor for stale-header regression\n");
            goto done;
        }
        cursor->header.pid = 0;
        cursor->header.size = 1;
        if (UDBRecords_nextRecord(db->records) != NULL ||
            NULL == db->records->file->file ||
            UDBRecord_pid(cursor) != (PID_TYPE)77 ||
            UDBRecord_size(cursor) != (PID_TYPE)sizeof(payload)) {
            fprintf(stderr, "record cursor trusted stale cached extent\n");
            goto done;
        }
    }

    /* A physical move must not trust a caller-modified cached extent. */
    {
        int move_accepted;

        if (JFile_logSize(db->records->file) != 0) {
            fprintf(stderr, "record move stale-header fixture had a pending journal\\n");
            goto done;
        }
        record->header.pid = 0;
        record->header.size = 1;
        move_accepted = UDBRecord_moveToPos_(record, saved_pos + 1);
        if (JFile_clipLog(db->records->file) != 0) {
            fprintf(stderr, "unable to discard uncommitted record-move fixture\\n");
            goto done;
        }
        clearerr(db->records->file->file);
        if (move_accepted) {
            fprintf(stderr, "record move trusted stale cached size\\n");
            goto done;
        }
        if (!UDBRecord_readHeader(record) ||
            UDBRecord_pid(record) != (PID_TYPE)77 ||
            UDBRecord_size(record) != (PID_TYPE)sizeof(payload)) {
            fprintf(stderr, "record move failure did not retain persisted metadata\\n");
            goto done;
        }
        restored = UDBRecord_readDatum(record);
        if (restored.data == NULL || restored.size != sizeof(payload) ||
            memcmp(restored.data, payload, sizeof(payload)) != 0) {
            fprintf(stderr, "record move failure damaged the source payload\\n");
            goto done;
        }
    }

    /*
     * Offset zero stores free-list metadata, not a physical record. Craft
     * metadata/header bytes that would otherwise decode as a valid EMPTY row.
     */
    {
        FILE *data_file = db->records->file->file;
        PID_TYPE saved_empty_pos, forged_empty_pos = (PID_TYPE)1;
        UDBRecordHeader saved_header, forged_header;
        int forged_lookup_accepted = 0;
        int restore_ok = 1;

        if (fseek(data_file, 0, SEEK_SET) != 0 ||
            fread(&saved_empty_pos, sizeof(saved_empty_pos), 1, data_file) != 1 ||
            fseek(data_file, (long)saved_pos, SEEK_SET) != 0 ||
            fread(&saved_header, sizeof(saved_header), 1, data_file) != 1) {
            fprintf(stderr, "unable to save record metadata for boundary test\n");
            goto done;
        }
        memset(&forged_header, 0, sizeof(forged_header));
        forged_header.tag = RECORD_TAG_EMPTY;
        forged_header.pid = (PID_TYPE)1;
        forged_header.size = (PID_TYPE)1;

        if (fseek(data_file, (long)saved_pos, SEEK_SET) != 0 ||
            fwrite(&forged_header, sizeof(forged_header), 1, data_file) != 1 ||
            fseek(data_file, 0, SEEK_SET) != 0 ||
            fwrite(&forged_empty_pos, sizeof(forged_empty_pos), 1, data_file) != 1 ||
            fflush(data_file) != 0) {
            if (fseek(data_file, (long)saved_pos, SEEK_SET) != 0 ||
                fwrite(&saved_header, sizeof(saved_header), 1, data_file) != 1)
                restore_ok = 0;
            if (fseek(data_file, 0, SEEK_SET) != 0 ||
                fwrite(&saved_empty_pos, sizeof(saved_empty_pos), 1, data_file) != 1)
                restore_ok = 0;
            if (fflush(data_file) != 0)
                restore_ok = 0;
            clearerr(data_file);
            fprintf(stderr, "unable to craft record metadata boundary fixture\n");
            if (!restore_ok)
                fprintf(stderr, "unable to restore record metadata boundary fixture\n");
            goto done;
        }

        forged_lookup_accepted =
            UDBRecords_recordAtPos_(db->records, (PID_TYPE)0) != NULL;

        /* Restore disk state before asserting so later cases see an intact DB. */
        if (fseek(data_file, (long)saved_pos, SEEK_SET) != 0 ||
            fwrite(&saved_header, sizeof(saved_header), 1, data_file) != 1 ||
            fseek(data_file, 0, SEEK_SET) != 0 ||
            fwrite(&saved_empty_pos, sizeof(saved_empty_pos), 1, data_file) != 1 ||
            fflush(data_file) != 0) {
            clearerr(data_file);
            fprintf(stderr, "unable to restore record metadata after boundary test\n");
            goto done;
        }
        clearerr(data_file);
        if (forged_lookup_accepted) {
            fprintf(stderr, "record cursor accepted metadata offset as a physical row\n");
            goto done;
        }
        if (UDBRecords_recordAtPos_(db->records, saved_pos) == NULL ||
            UDBRecord_pid(db->records->record) != (PID_TYPE)77) {
            fprintf(stderr, "record cursor did not recover after metadata boundary rejection\n");
            goto done;
        }
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

    /* Offset zero stores free-list metadata, never a physical record. */
    if (UDBRecord_moveToPos_(record, 0) ||
        !UDBRecord_readHeader(record) ||
        UDBRecord_pid(record) != (PID_TYPE)77 ||
        UDBRecord_size(record) != (PID_TYPE)sizeof(payload)) {
        fprintf(stderr, "record move accepted reserved metadata offset\n");
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

    /* Removal must ignore stale cached PID/size and preserve on-disk metadata. */
    record->header.pid = 0;
    record->header.size = 0;
    UDB_beginTransaction(db);
    if (!UDBRecord_remove(record)) {
        fprintf(stderr, "record tombstone write failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !UDBRecord_readHeader(record) ||
        !UDBRecord_isEmpty(record) || UDBRecord_pid(record) != 77 ||
        UDBRecord_size(record) != sizeof(payload)) {
        fprintf(stderr, "record tombstone did not preserve persisted PID/extent\n");
        goto done;
    }

    /*
     * UDBRecords record cursors are scoped to one JFile. A foreign record
     * must never be interpreted at the same byte offset in this database,
     * or tombstoned through the other database's file handle.
     */
    foreign_db = UDB_new();
    if (NULL == foreign_db) {
        fprintf(stderr, "foreign UDB allocation failed\n");
        goto done;
    }
    foreign_path_len = snprintf(foreign_path, sizeof(foreign_path),
        "udb_record_storage_foreign_%ld", (long)getpid());
    if (foreign_path_len < 0 ||
        (size_t)foreign_path_len >= sizeof(foreign_path)) {
        fprintf(stderr, "unable to construct foreign database path\n");
        goto done;
    }
    UDB_setPath_(foreign_db, foreign_path);
    UDB_delete(foreign_db);
    UDB_open(foreign_db);
    if (!UDB_isOpen(foreign_db)) {
        fprintf(stderr, "foreign UDB open failed\n");
        goto done;
    }
    {
        unsigned char foreign_payload = 0x7b;
        Datum foreign_datum = {1, &foreign_payload};
        PID_TYPE foreign_pid;
        UDB_beginTransaction(foreign_db);
        foreign_pid = UDB_put_(foreign_db, foreign_datum);
        if (!UDB_isOpen(foreign_db) || foreign_pid == 0) {
            fprintf(stderr, "foreign UDB fixture append failed\n");
            goto done;
        }
        UDB_commitTransaction(foreign_db);
        if (!UDB_isOpen(foreign_db)) {
            fprintf(stderr, "foreign UDB fixture commit failed\n");
            goto done;
        }
    }
    foreign_record = UDBRecords_firstRecord(foreign_db->records);
    if (NULL == foreign_record || foreign_record->file != foreign_db->records->file ||
        UDBRecord_size(foreign_record) != 1 || UDBRecord_isEmpty(foreign_record)) {
        fprintf(stderr, "foreign UDB record fixture was not readable\n");
        goto done;
    }

    /* The foreign record ends inside this DB's longer record payload. */
    if (UDBRecords_recordAfter_(db->records, foreign_record) != NULL ||
        !UDB_isOpen(db)) {
        fprintf(stderr, "UDBRecords_recordAfter_ accepted a foreign record file\n");
        goto done;
    }

    /* Make the foreign file writable so a missing owner guard is observable. */
    UDB_beginTransaction(foreign_db);
    if (UDBRecords_removeRecord_(db->records, foreign_record) != 0 ||
        !UDBRecord_readHeader(foreign_record) ||
        UDBRecord_isEmpty(foreign_record)) {
        fprintf(stderr, "UDBRecords_removeRecord_ mutated a foreign record\n");
        goto done;
    }
    UDB_commitTransaction(foreign_db);
    if (!UDB_isOpen(foreign_db) || !UDB_isOpen(db)) {
        fprintf(stderr, "cross-store record rejection damaged a database\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("udb record storage test passed");

done:
    if (foreign_record && (!foreign_db || foreign_record != foreign_db->records->record))
        UDBRecord_free(foreign_record);
    if (clone) UDBRecord_free(clone);
    if (probe) UDBRecord_free(probe);
    if (record) UDBRecord_free(record);
    UDB_delete(foreign_db);
    UDB_free(foreign_db);
    UDB_delete(db);
    UDB_free(db);
    return result;
}
