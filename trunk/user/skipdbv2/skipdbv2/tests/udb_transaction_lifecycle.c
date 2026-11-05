#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int has_value(UDB *db, PID_TYPE pid, const char *expected)
{
    Datum value = UDB_at_(db, pid);
    size_t expected_size = strlen(expected) + 1;

    return value.data != NULL && value.size == expected_size &&
        memcmp(value.data, expected, expected_size) == 0;
}

int main(void)
{
    UDB *db = UDB_new();
    PID_TYPE pid = 0;
    char path[160];
    int path_len;
    int result = EXIT_FAILURE;

    if (db == NULL) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }

    path_len = snprintf(path, sizeof(path), "udb_lifecycle_test_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        fprintf(stderr, "unable to construct test path\n");
        goto done;
    }

    UDB_setPath_(db, path);
    if (UDB_path(db) == NULL || strcmp(UDB_path(db), path) != 0 ||
        db->index == NULL || db->index->file == NULL ||
        db->records == NULL || db->records->file == NULL ||
        db->commitMarker.path == NULL ||
        strstr(db->index->file->path, ".udbIndex") == NULL ||
        strstr(db->records->file->path, ".udbData") == NULL ||
        strstr(db->commitMarker.path, ".committed") == NULL) {
        fprintf(stderr, "database path ownership/configuration failed\n");
        goto done;
    }

    UDB_delete(db);
    UDB_open(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "database open failed\n");
        goto done;
    }
    UDB_open(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "repeated open changed a live database state\n");
        goto done;
    }

    UDB_beginTransaction(db);
    if (!UDB_isOpen(db) || !db->withinTransaction) {
        fprintf(stderr, "transaction begin failed\n");
        goto done;
    }
    UDB_beginTransaction(db);
    if (!UDB_isOpen(db) || !db->withinTransaction) {
        fprintf(stderr, "repeated begin corrupted transaction state\n");
        goto done;
    }

    pid = UDB_put_(db, Datum_FromCString_("lifecycle-value"));
    if (pid == 0 || !UDB_isOpen(db) || !db->withinTransaction) {
        fprintf(stderr, "transactional append failed\n");
        goto done;
    }

    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || db->withinTransaction ||
        !has_value(db, pid, "lifecycle-value")) {
        fprintf(stderr, "two-file transaction commit failed\n");
        goto done;
    }

    UDB_close(db);
    if (UDB_isOpen(db)) {
        fprintf(stderr, "database close left the instance open\n");
        goto done;
    }
    UDB_open(db);
    if (!UDB_isOpen(db) || !has_value(db, pid, "lifecycle-value")) {
        fprintf(stderr, "committed value did not survive close/reopen\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("udb transaction lifecycle test passed");

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
