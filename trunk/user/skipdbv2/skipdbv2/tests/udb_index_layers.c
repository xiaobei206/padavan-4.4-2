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
    PID_TYPE first = 0, second = 0, reused = 0, abandoned = 0;
    char path[128];
    int path_len, result = EXIT_FAILURE;

    if (db == NULL) {
        fprintf(stderr, "UDB_new failed\n");
        return EXIT_FAILURE;
    }
    path_len = snprintf(path, sizeof(path), "udb_index_layers_test_%ld",
        (long)getpid());
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        fprintf(stderr, "unable to construct test path\n");
        goto done;
    }
    UDB_setPath_(db, path);
    UDB_delete(db);
    UDB_open(db);
    if (!UDB_isOpen(db) || UDBIndex_nextPid(db->index) != 1) {
        fprintf(stderr, "fresh index did not establish its sentinel slot\n");
        goto done;
    }
    if (UDBIndex_posForPid_(db->index, (PID_TYPE)-1) != 0 ||
        UDBIndex_setPos_forPid_(db->index, 1, (PID_TYPE)-1) != 0 ||
        UDBIndex_nextPid(db->index) != 1) {
        fprintf(stderr, "overflow PID offset was accepted\n");
        goto done;
    }

    UDB_beginTransaction(db);
    first = UDB_allocPid(db);
    second = UDB_allocPid(db);
    if (!UDB_isOpen(db) || first != 1 || second != 2 ||
        !UDBIndex_isReservedPid_(db->index, first) ||
        !UDBIndex_isReservedPid_(db->index, second)) {
        fprintf(stderr, "allocator did not reserve consecutive PIDs\n");
        goto done;
    }
    UDB_at_put_(db, first, Datum_FromCString_("first"));
    UDB_at_put_(db, second, Datum_FromCString_("second"));
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || db->index->reserved->size != 0 ||
        UDBIndex_isReservedPid_(db->index, first) ||
        !has_value(db, first, "first") || !has_value(db, second, "second") ||
        UDBIndex_nextPid(db->index) != 3) {
        fprintf(stderr, "committed index retained reservations or lost mappings\n");
        goto done;
    }

    UDB_beginTransaction(db);
    abandoned = UDB_allocPid(db);
    if (!UDB_isOpen(db) || abandoned != 3 ||
        !UDBIndex_isReservedPid_(db->index, abandoned)) {
        fprintf(stderr, "unwritten reservation was not allocated\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || db->index->reserved->size != 0 ||
        UDBIndex_nextPid(db->index) != abandoned) {
        fprintf(stderr, "unwritten reservation was not reclaimed at commit\n");
        goto done;
    }

    UDB_beginTransaction(db);
    UDB_removeAt_(db, first);
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || UDBIndex_nextPid(db->index) != first ||
        UDB_at_(db, first).data != NULL) {
        fprintf(stderr, "released PID was not returned to the free stack\n");
        goto done;
    }

    UDB_beginTransaction(db);
    reused = UDB_allocPid(db);
    if (!UDB_isOpen(db) || reused != first ||
        !UDBIndex_isReservedPid_(db->index, reused)) {
        fprintf(stderr, "allocator did not reserve the released PID\n");
        goto done;
    }
    UDB_at_put_(db, reused, Datum_FromCString_("reused"));
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || db->index->reserved->size != 0 ||
        UDBIndex_isReservedPid_(db->index, reused) ||
        !has_value(db, reused, "reused") || UDBIndex_nextPid(db->index) != 3) {
        fprintf(stderr, "reused PID remained in a stale free/reserved state\n");
        goto done;
    }

    UDB_close(db);
    UDB_open(db);
    if (!UDB_isOpen(db) || !has_value(db, reused, "reused") ||
        !has_value(db, second, "second") || UDBIndex_nextPid(db->index) != 3) {
        fprintf(stderr, "index state did not survive a rebuild on reopen\n");
        goto done;
    }
    result = EXIT_SUCCESS;

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
