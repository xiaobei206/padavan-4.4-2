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
    if (!record_key_is(record, "gamma")) {
        fprintf(stderr, "cursor failed to locate gamma after reopen\n");
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (!record_key_is(record, "beta")) {
        fprintf(stderr, "previous(gamma) did not return beta after reopen\n");
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (!record_key_is(record, "alpha")) {
        fprintf(stderr, "previous(beta) did not return alpha after reopen\n");
        goto done;
    }

    record = SkipDBCursor_previous(cursor);
    if (record != NULL) {
        fprintf(stderr, "previous(first record) did not stop at the header\n");
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
