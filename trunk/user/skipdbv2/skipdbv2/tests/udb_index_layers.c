#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
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
    if (UDBIndex_setPos_forPid_(db->index, 1, 2) != 0 ||
        UDBIndex_nextPid(db->index) != 1) {
        fprintf(stderr, "unreserved PID mapping changed index state\n");
        goto done;
    }

    /*
     * The last PID whose slot begins below LONG_MAX cannot be allocated if
     * the full fixed-width index entry would extend past the seekable range.
     */
    {
        PID_TYPE previous_max_pid = db->index->maxPid;
        PID_TYPE max_seekable_pid =
            (PID_TYPE)((size_t)LONG_MAX / sizeof(UDBIndexEntry));
        PID_TYPE preview_pid, allocated_pid;
        long previous_max_pos = db->index->file->maxPos;
        long previous_file_pos = db->index->file->pos;
        long boundary_end = (long)(max_seekable_pid * sizeof(UDBIndexEntry));

        if (max_seekable_pid < 2) {
            fprintf(stderr, "seek-range regression is unsupported on this target\n");
            goto done;
        }
        db->index->maxPid = max_seekable_pid - 1;
        db->index->file->maxPos = boundary_end;
        preview_pid = UDBIndex_nextPid(db->index);
        allocated_pid = UDBIndex_allocPid(db->index);
        db->index->maxPid = previous_max_pid;
        db->index->file->maxPos = previous_max_pos;
        db->index->file->pos = previous_file_pos;
        if (preview_pid != 0) {
            fprintf(stderr, "next-PID preview returned a slot beyond LONG_MAX\n");
            goto done;
        }
        if (allocated_pid != 0 || db->index->reserved->size != 0) {
            fprintf(stderr, "allocator returned a PID whose index slot exceeds LONG_MAX\n");
            goto done;
        }
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
    if (UDBIndex_setPos_forPid_(db->index,
            (PID_TYPE)(sizeof(PID_TYPE) - 1), first) != 0 ||
        !UDBIndex_isReservedPid_(db->index, first) ||
        !UDBIndex_isReservedPid_(db->index, second)) {
        fprintf(stderr, "index accepted a record offset before the data header\\n");
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
    UDB_at_put_(db, first, Datum_FromCString_("first-replaced"));
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, first, "first-replaced") ||
        !has_value(db, second, "second")) {
        fprintf(stderr, "existing PID mapping could not be updated after commit\n");
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

    /* A closed index is unavailable, not an empty PID mapping; do not
     * mutate the cached seek cursor when refusing a read from a closed file. */
    JFile_setPosition_(db->index->file, 0);
    {
        long saved_pos = db->index->file->pos;
        long saved_max = db->index->file->maxPos;

        UDB_close(db);
        if (UDBIndex_posForPid_(db->index, reused) != 0 ||
            db->index->file->pos != saved_pos ||
            db->index->file->maxPos != saved_max) {
            fprintf(stderr, "closed index lookup changed the cached file position\n");
            goto done;
        }
    }
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
