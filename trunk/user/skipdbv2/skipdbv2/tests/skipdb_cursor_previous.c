#include "SkipDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int record_key_is(SkipDBRecord *record, const char *expected)
{
    Datum key;
    size_t expected_size;

    if (NULL == record || NULL == expected)
        return 0;
    key = SkipDBRecord_keyDatum(record);
    expected_size = strlen(expected) + 1;
    return key.data != NULL && key.size == expected_size &&
        memcmp(key.data, expected, expected_size) == 0;
}

int main(void)
{
    SkipDB *db = NULL;
    SkipDBCursor *cursor = NULL;
    SkipDBRecord *record;
    char path[128];
    int path_len;
    int result = EXIT_FAILURE;

    db = SkipDB_new();
    if (NULL == db) {
        fprintf(stderr, "SkipDB_new failed\n");
        return EXIT_FAILURE;
    }

    path_len = snprintf(path, sizeof(path), "skipdb_cursor_previous_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        fprintf(stderr, "unable to construct test database path\n");
        goto done;
    }

    SkipDB_setPath_(db, path);
    SkipDB_delete(db);
    if (!SkipDB_open(db) || !SkipDB_isOpen(db)) {
        fprintf(stderr, "new SkipDB did not open\n");
        goto done;
    }

    SkipDB_beginTransaction(db);
    if (!SkipDB_isOpen(db) ||
        !SkipDB_at_put_(db, Datum_FromCString_("alpha"),
            Datum_FromCString_("1")) ||
        !SkipDB_at_put_(db, Datum_FromCString_("beta"),
            Datum_FromCString_("2")) ||
        !SkipDB_at_put_(db, Datum_FromCString_("gamma"),
            Datum_FromCString_("3")) ||
        !SkipDB_isOpen(db)) {
        fprintf(stderr, "failed to insert cursor test records\n");
        goto done;
    }
    SkipDB_commitTransaction(db);
    if (!SkipDB_isOpen(db)) {
        fprintf(stderr, "failed to commit cursor test records\n");
        goto done;
    }

    /* Reverse links are not persisted, so force every record to reload. */
    SkipDB_close(db);
    if (SkipDB_isOpen(db) || !SkipDB_open(db) || !SkipDB_isOpen(db)) {
        fprintf(stderr, "database failed to reopen\n");
        goto done;
    }

    cursor = SkipDB_createCursor(db);
    if (NULL == cursor) {
        fprintf(stderr, "failed to create cursor\n");
        goto done;
    }

    record = SkipDBCursor_goto_(cursor, Datum_FromCString_("gamma"));
    if (!record_key_is(record, "gamma") || SkipDBCursor_current(cursor) != record) {
        fprintf(stderr, "cursor failed to locate gamma after reopen\n");
        goto done;
    }

    /* Closing the database invalidates every cursor's borrowed record pointer. */
    SkipDB_close(db);
    if (SkipDB_isOpen(db) ||
        SkipDBCursor_current(cursor) != NULL ||
        SkipDBCursor_next(cursor) != NULL ||
        SkipDBCursor_previous(cursor) != NULL ||
        SkipDBCursor_goto_(cursor, Datum_FromCString_("gamma")) != NULL) {
        fprintf(stderr, "cursor retained a record after database close\n");
        goto done;
    }
    if (!SkipDB_open(db) || !SkipDB_isOpen(db)) {
        fprintf(stderr, "database failed to reopen after cursor invalidation\n");
        goto done;
    }
    record = SkipDBCursor_goto_(cursor, Datum_FromCString_("gamma"));
    if (!record_key_is(record, "gamma")) {
        fprintf(stderr, "cursor did not recover after database reopen\n");
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (!record_key_is(record, "beta")) {
        fprintf(stderr, "previous(gamma) did not return beta after reopen\n");
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (!record_key_is(record, "alpha")) {
        Datum actual_key = SkipDBRecord_keyDatum(record);
        SkipDBRecord *update0 = db->update[0];
        Datum update_key = SkipDBRecord_keyDatum(update0);
        fprintf(stderr,
            "previous(beta) did not return alpha after reopen: "
            "open=%d returned=%p pid=%lu key=%.*s level=%d "
            "update0=%p pid=%lu key=%.*s header=%p headerPid=%lu\n",
            SkipDB_isOpen(db), (void *)record,
            (unsigned long)SkipDBRecord_pid(record),
            (int)actual_key.size,
            actual_key.data ? (const char *)actual_key.data : "",
            SkipDBRecord_level(record), (void *)update0,
            (unsigned long)SkipDBRecord_pid(update0),
            (int)update_key.size,
            update_key.data ? (const char *)update_key.data : "",
            (void *)SkipDB_headerRecord(db),
            (unsigned long)SkipDB_headerPid(db));
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (record != NULL) {
        fprintf(stderr, "previous(first record) did not stop at the header\n");
        goto done;
    }

    /* Forward traversal also remains bounded and usable after repositioning. */
    record = SkipDBCursor_first(cursor);
    if (!record_key_is(record, "alpha") ||
        !record_key_is(SkipDBCursor_next(cursor), "beta") ||
        !record_key_is(SkipDBCursor_next(cursor), "gamma") ||
        SkipDBCursor_next(cursor) != NULL) {
        fprintf(stderr, "forward cursor traversal failed after repositioning\n");
        goto done;
    }

    /* A non-root row with level zero has no forward-link slot and is
     * malformed. Persist one deliberately and ensure traversal fails closed. */
    record = SkipDB_recordAt_(db, Datum_FromCString_("gamma"));
    if (NULL == record) {
        fprintf(stderr, "failed to load gamma for corruption regression test\n");
        goto done;
    }
    SkipDB_beginTransaction(db);
    SkipDBRecord_level_(record, 0);
    SkipDBRecord_markAsDirty(record);
    SkipDB_commitTransaction(db);
    if (!SkipDB_isOpen(db)) {
        fprintf(stderr, "failed to persist malformed-row regression fixture\n");
        goto done;
    }
    /* The zero-level row is still cached here. Last-record traversal must
     * reject it instead of treating the empty level loop as a valid tail. */
    if (SkipDB_lastRecord(db) != NULL || SkipDB_isOpen(db)) {
        fprintf(stderr, "last-record traversal accepted a cached level-zero row\n");
        goto done;
    }
    SkipDB_close(db);
    if (SkipDB_isOpen(db) || !SkipDB_open(db) || !SkipDB_isOpen(db)) {
        fprintf(stderr, "database failed to reopen after cached level-zero rejection\n");
        goto done;
    }
    if (SkipDB_count(db) != -1 || SkipDB_isOpen(db)) {
        fprintf(stderr, "malformed level-zero row was not rejected\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("skipdb cursor previous test passed");

done:
    if (cursor)
        SkipDBCursor_release(cursor);
    if (db) {
        SkipDB_delete(db);
        SkipDB_free(db);
    }
    return result;
}
