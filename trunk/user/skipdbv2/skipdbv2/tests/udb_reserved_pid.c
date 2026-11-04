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
    PID_TYPE first_pid, second_pid, reused_pid, abandoned_pid;
    char path[128];
    int result = EXIT_FAILURE;
    int path_len;

    db = UDB_new();
    if (NULL == db) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }

    path_len = snprintf(path, sizeof(path), "udb_reserved_pid_test_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
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

    /* An allocated PID that is never written must expire at commit. */
    UDB_beginTransaction(db);
    abandoned_pid = UDB_allocPid(db);
    if (!UDB_isOpen(db) || abandoned_pid == 0) {
        fprintf(stderr, "failed to reserve the intentionally unused PID\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "commit of an unused reservation failed\n");
        goto done;
    }

    /* The expired PID cannot be written until the allocator reserves it again. */
    UDB_beginTransaction(db);
    UDB_at_put_(db, abandoned_pid, Datum_FromCString_("stale-reservation"));
    if (UDB_isOpen(db)) {
        fprintf(stderr, "a PID from a completed transaction remained reserved\n");
        goto done;
    }

    /* Reopening rebuilds the hole; a fresh reservation may reuse it safely. */
    UDB_open(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "database failed to reopen after stale PID rejection\n");
        goto done;
    }
    UDB_beginTransaction(db);
    if ((reused_pid = UDB_allocPid(db)) != abandoned_pid) {
        fprintf(stderr, "expired PID was not returned to the free pool\n");
        goto done;
    }
    UDB_at_put_(db, reused_pid, Datum_FromCString_("reused-after-expiry"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "freshly reserved expired PID could not be written\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, reused_pid, "reused-after-expiry")) {
        fprintf(stderr, "freshly reused PID did not persist\n");
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
