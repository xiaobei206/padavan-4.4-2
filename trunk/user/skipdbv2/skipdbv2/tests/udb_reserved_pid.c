#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int has_value(UDB *db, PID_TYPE pid, const char *expected)
{
    Datum value;
    size_t expected_size;

    value = UDB_at_(db, pid);
    expected_size = strlen(expected) + 1;
    return value.data != NULL && value.size == expected_size &&
        memcmp(value.data, expected, expected_size) == 0;
}

int main(void)
{
    UDB *db;
    PID_TYPE first_pid, second_pid, reused_pid;
    char path[128];
    int result = EXIT_FAILURE;

    db = UDB_new();
    if (NULL == db) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }

    if (snprintf(path, sizeof(path), "udb_reserved_pid_test_%ld",
            (long)getpid()) < 0) {
        fprintf(stderr, "unable to construct test database path\n");
        goto done;
    }
    UDB_setPath_(db, path);
    UDB_delete(db);
    UDB_open(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "new database did not open\n");
        goto done;
    }

    /* A transaction may reserve several IDs before writing any of them. */
    UDB_beginTransaction(db);
    first_pid = UDB_allocPid(db);
    second_pid = UDB_allocPid(db);
    if (!UDB_isOpen(db) || first_pid == 0 || second_pid == 0 ||
        first_pid == second_pid) {
        fprintf(stderr, "failed to reserve distinct PIDs\n");
        goto done;
    }
    UDB_at_put_(db, first_pid, Datum_FromCString_("first"));
    UDB_at_put_(db, second_pid, Datum_FromCString_("second"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "first writes to reserved PIDs were rejected\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "first") ||
        !has_value(db, second_pid, "second")) {
        fprintf(stderr, "reserved PID records did not persist\n");
        goto done;
    }

    /* Updating a mapped PID must retire its old row without losing the value. */
    UDB_beginTransaction(db);
    UDB_at_put_(db, first_pid, Datum_FromCString_("updated"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "updating an existing PID failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated")) {
        fprintf(stderr, "updated PID value was not committed\n");
        goto done;
    }

    /* A freed PID can be reserved and written again in a later transaction. */
    UDB_beginTransaction(db);
    UDB_removeAt_(db, second_pid);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "removing an existing PID failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || UDB_at_(db, second_pid).data != NULL) {
        fprintf(stderr, "removed PID is still readable\n");
        goto done;
    }

    UDB_beginTransaction(db);
    reused_pid = UDB_allocPid(db);
    if (!UDB_isOpen(db) || reused_pid != second_pid) {
        fprintf(stderr, "allocator did not reuse the freed PID\n");
        goto done;
    }
    UDB_at_put_(db, reused_pid, Datum_FromCString_("reused"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "first write to a reused PID failed\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, reused_pid, "reused")) {
        fprintf(stderr, "reused PID value was not committed\n");
        goto done;
    }

    /* A gap not reserved by allocPid must still be rejected. */
    UDB_beginTransaction(db);
    UDB_at_put_(db, (PID_TYPE)100, Datum_FromCString_("unreserved"));
    if (UDB_isOpen(db)) {
        fprintf(stderr, "an arbitrary unmapped PID was accepted\n");
        goto done;
    }

    /* The rejected transaction must not damage previously committed rows. */
    UDB_open(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
        !has_value(db, reused_pid, "reused")) {
        fprintf(stderr, "committed data was damaged by rejected PID write\n");
        goto done;
    }

    result = EXIT_SUCCESS;

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
