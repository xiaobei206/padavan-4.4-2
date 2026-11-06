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

    /*
     * Corrupt the payload-size field of the first pending index-journal
     * record while retaining the final item terminator. Pre-commit syncing
     * can still close the transaction, but journal validation must reject
     * it before the durable cross-file commit marker is armed.
     */
    {
        PID_TYPE rejected_pid;
        FILE *journal;
        size_t zero_size = 0;

        UDB_beginTransaction(db);
        rejected_pid = UDB_put_(db, Datum_FromCString_("must-not-commit"));
        if (!UDB_isOpen(db) || rejected_pid == 0 ||
            !db->withinTransaction || fflush(db->index->file->log) != 0) {
            fprintf(stderr, "unable to prepare malformed-journal fixture\n");
            goto done;
        }

        journal = fopen(db->index->file->logPath, "r+b");
        if (journal == NULL) {
            fprintf(stderr, "unable to open index journal for corruption test\n");
            goto done;
        }
        if (fseek(journal, (long)sizeof(long), SEEK_SET) != 0 ||
            fwrite(&zero_size, sizeof(zero_size), 1, journal) != 1 ||
            fflush(journal) != 0 || fsync(fileno(journal)) != 0) {
            (void)fclose(journal);
            fprintf(stderr, "unable to corrupt index journal fixture\n");
            goto done;
        }
        if (fclose(journal) != 0) {
            fprintf(stderr, "unable to close corrupted index journal fixture\n");
            goto done;
        }

        UDB_commitTransaction(db);
        if (UDB_isOpen(db)) {
            fprintf(stderr, "malformed journal was accepted for commit\n");
            goto done;
        }

        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, pid, "lifecycle-value") ||
            UDB_at_(db, rejected_pid).data != NULL) {
            fprintf(stderr, "failed transaction became visible after reopen\n");
            goto done;
        }
    }

    result = EXIT_SUCCESS;
    puts("udb transaction lifecycle test passed");

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
