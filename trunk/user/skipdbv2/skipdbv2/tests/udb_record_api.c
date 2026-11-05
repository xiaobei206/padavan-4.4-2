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
    UDB *db = NULL;
    Datum invalid;
    PID_TYPE pid = 0;
    char path[128];
    int path_len;
    int result = EXIT_FAILURE;

    db = UDB_new();
    if (NULL == db) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }
    path_len = snprintf(path, sizeof(path), "udb_record_api_test_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        fprintf(stderr, "unable to construct test database path\n");
        goto done;
    }

    UDB_setPath_(db, path);
    UDB_delete(db);
    UDB_open(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "database did not open\n");
        goto done;
    }

    /* Writes and PID reservations are transaction-only operations. */
    invalid.size = 1;
    invalid.data = NULL;
    if (UDB_allocPid(db) != 0 ||
        UDB_put_(db, Datum_FromCString_("outside-transaction")) != 0 ||
        !UDB_isOpen(db)) {
        fprintf(stderr, "write was accepted outside a transaction\n");
        goto done;
    }

    UDB_beginTransaction(db);
    if (!UDB_isOpen(db) || UDB_put_(db, invalid) != 0 ||
        !UDB_isOpen(db)) {
        fprintf(stderr, "invalid datum was not rejected without poisoning the transaction\n");
        goto done;
    }
    pid = UDB_put_(db, Datum_FromCString_("first-value"));
    if (pid == 0 || !UDB_isOpen(db)) {
        fprintf(stderr, "valid append failed after invalid datum rejection\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, pid, "first-value")) {
        fprintf(stderr, "appended record did not persist\n");
        goto done;
    }

    UDB_beginTransaction(db);
    UDB_at_put_(db, pid, Datum_FromCString_("updated-value"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "record replacement failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, pid, "updated-value")) {
        fprintf(stderr, "replacement value did not persist\n");
        goto done;
    }

    UDB_beginTransaction(db);
    UDB_removeAt_(db, pid);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "record removal failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || UDB_at_(db, pid).data != NULL) {
        fprintf(stderr, "removed record remained readable\n");
        goto done;
    }

    UDB_close(db);
    UDB_open(db);
    if (!UDB_isOpen(db) || UDB_at_(db, pid).data != NULL) {
        fprintf(stderr, "removed record reappeared after reopen\n");
        goto done;
    }

    result = EXIT_SUCCESS;

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
