#include "UDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The marker component is armed directly to simulate a power loss. */

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
    char marker_path[160];
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

    /* Corrupt the persisted hole offset to point one byte into the first
     * row. That byte resembles RECORD_TAG_EMPTY on little-endian systems;
     * opening must recover a real record boundary before compaction. */
    {
        FILE *data_file;
        char data_path[160];
        PID_TYPE fake_pos = (PID_TYPE)sizeof(PID_TYPE) + 1;
        int data_path_len = snprintf(data_path, sizeof(data_path),
            "%s.udbData", path);
        if (data_path_len < 0 || (size_t)data_path_len >= sizeof(data_path)) {
            fprintf(stderr, "unable to construct UDB data path\n");
            goto done;
        }
        UDB_close(db);
        data_file = fopen(data_path, "r+b");
        if (NULL == data_file) {
            fprintf(stderr, "unable to corrupt first-empty metadata for test\n");
            goto done;
        }
        if (fwrite(&fake_pos, sizeof(fake_pos), 1, data_file) != 1 ||
            fflush(data_file) != 0) {
            fclose(data_file);
            fprintf(stderr, "unable to write corrupt first-empty metadata\n");
            goto done;
        }
        if (fclose(data_file) != 0) {
            fprintf(stderr, "unable to close corrupt metadata fixture\n");
            goto done;
        }
        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
            !has_value(db, second_pid, "second")) {
            fprintf(stderr, "invalid free-list offset damaged live records\n");
            goto done;
        }
    }

    /* "second" is one byte larger than the preceding "first" row. The
     * compactor must not overwrite it when the first hole is too small. */
    if (UDB_compactStep(db) != 0 || !UDB_isOpen(db) ||
        !has_value(db, first_pid, "updated") ||
        !has_value(db, second_pid, "second")) {
        fprintf(stderr, "compaction damaged a row larger than the free extent\n");
        goto done;
    }

    /* Compacting adjacent old versions must not clear PIDs that now map
     * to the newer, live records. */
    UDB_beginTransaction(db);
    UDB_at_put_(db, second_pid, Datum_FromCString_("second-updated"));
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
        !has_value(db, second_pid, "second-updated")) {
        fprintf(stderr, "updating the second PID failed before compaction\n");
        goto done;
    }
    if (UDB_compactStep(db) != 1 || !UDB_isOpen(db) ||
        !has_value(db, first_pid, "updated") ||
        !has_value(db, second_pid, "second-updated")) {
        fprintf(stderr, "compaction cleared a PID still mapped to live data\n");
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

    /* Compaction moves a live row into the leading hole, merges the
     * adjacent empty extent, then safely truncates the trailing hole. */
    if (UDB_compactStep(db) != 1 || !UDB_isOpen(db) ||
        !has_value(db, first_pid, "updated") ||
        UDB_at_(db, second_pid).data != NULL) {
        fprintf(stderr, "compaction failed to move a live row into a hole\n");
        goto done;
    }
    if (UDB_compactStep(db) != 1 || !UDB_isOpen(db) ||
        !has_value(db, first_pid, "updated") ||
        UDB_at_(db, second_pid).data != NULL) {
        fprintf(stderr, "compaction failed to merge adjacent empty extents\n");
        goto done;
    }
    if (UDB_compactStep(db) != 0 || !UDB_isOpen(db) ||
        !has_value(db, first_pid, "updated") ||
        UDB_at_(db, second_pid).data != NULL) {
        fprintf(stderr, "trailing-hole truncation damaged live records\n");
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

    /* A new reservation cleared before commit must return to the free pool. */
    UDB_beginTransaction(db);
    abandoned_pid = UDB_allocPid(db);
    if (!UDB_isOpen(db) || abandoned_pid == 0 ||
        !UDBIndex_setPos_forPid_(db->index, 0, abandoned_pid) ||
        !UDB_isOpen(db)) {
        fprintf(stderr, "failed to release an unwritten PID reservation\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "commit failed after releasing an unwritten reservation\n");
        goto done;
    }
    UDB_beginTransaction(db);
    reused_pid = UDB_allocPid(db);
    if (!UDB_isOpen(db) || reused_pid != abandoned_pid) {
        fprintf(stderr, "PID released before its first write was not reusable\n");
        goto done;
    }
    UDB_at_put_(db, reused_pid, Datum_FromCString_("reused-before-first-write"));
    if (!UDB_isOpen(db)) {
        fprintf(stderr, "released-before-write PID could not be reused\n");
        goto done;
    }
    UDB_commitTransaction(db);
    if (!UDB_isOpen(db) ||
        !has_value(db, reused_pid, "reused-before-first-write")) {
        fprintf(stderr, "reused-before-write PID value did not persist\n");
        goto done;
    }

    /* A direct attempt to free an arbitrary unmapped PID must not create
     * an index entry or poison the free-PID pool. */
    UDB_beginTransaction(db);
    if (UDBIndex_setPos_forPid_(db->index, 0, (PID_TYPE)100) ||
        !UDB_isOpen(db)) {
        fprintf(stderr, "an arbitrary unmapped PID was added to the free pool\n");
        goto done;
    }
    UDB_close(db);
    UDB_open(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
        !has_value(db, reused_pid, "reused-before-first-write")) {
        fprintf(stderr, "rejected PID release damaged committed rows\n");
        goto done;
    }

    /* A gap not reserved by allocPid must still be rejected for writes. */
    UDB_beginTransaction(db);
    UDB_at_put_(db, (PID_TYPE)100, Datum_FromCString_("unreserved"));
    if (UDB_isOpen(db)) {
        fprintf(stderr, "an arbitrary unmapped PID was accepted\n");
        goto done;
    }

    /* The rejected transaction must not damage previously committed rows. */
    UDB_open(db);
    if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
        !has_value(db, reused_pid, "reused-before-first-write")) {
        fprintf(stderr, "committed data was damaged by rejected PID write\n");
        goto done;
    }

    /* Simulate a crash after applying the index file but before applying data. */
    {
        PID_TYPE crash_pid;
        UDB_beginTransaction(db);
        crash_pid = UDB_put_(db, Datum_FromCString_("crash-recovery-index"));
        if (!UDB_isOpen(db) || crash_pid == 0) {
            fprintf(stderr, "failed to stage the index-recovery transaction\n");
            goto done;
        }
        UDBIndex_preCommit(db->index);
        UDBRecords_preCommit(db->records);
        if (!UDB_isOpen(db) || !UDBIndex_prepareCommit(db->index) ||
            UDBCommitMarker_write(&db->commitMarker, 1) != 0) {
            fprintf(stderr, "failed to arm the index-recovery transaction\n");
            goto done;
        }
        UDBIndex_commit(db->index);
        if (!db->index->file->file || !db->index->file->log ||
            !db->records->file->file || !db->records->file->log) {
            fprintf(stderr, "index journal apply failed before crash simulation\n");
            goto done;
        }
        UDB_close(db);
        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, crash_pid, "crash-recovery-index")) {
            fprintf(stderr, "recovery failed after index-only journal application\n");
            goto done;
        }
    }

    /* A missing or truncated commit marker must not silently discard recovery state. */
    {
        FILE *marker;
        int marker_path_len = snprintf(marker_path, sizeof(marker_path),
            "%s.committed", path);
        if (marker_path_len < 0 || (size_t)marker_path_len >= sizeof(marker_path)) {
            fprintf(stderr, "unable to construct commit marker path\n");
            goto done;
        }

        UDB_close(db);
        if (remove(marker_path) != 0) {
            fprintf(stderr, "unable to remove marker for missing-marker regression\n");
            goto done;
        }
        UDB_open(db);
        if (UDB_isOpen(db)) {
            fprintf(stderr, "database opened after its commit marker was removed\n");
            goto done;
        }

        marker = fopen(marker_path, "w");
        if (NULL == marker || fputc(0, marker) == EOF) {
            if (marker) fclose(marker);
            fprintf(stderr, "unable to restore commit marker after missing-marker check\n");
            goto done;
        }
        if (fclose(marker) != 0) {
            fprintf(stderr, "unable to close restored commit marker\n");
            goto done;
        }
        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
            !has_value(db, reused_pid, "reused-before-first-write")) {
            fprintf(stderr, "valid marker did not reopen data after missing-marker rejection\n");
            goto done;
        }

        UDB_close(db);
        marker = fopen(marker_path, "w");
        if (NULL == marker) {
            fprintf(stderr, "unable to truncate marker for empty-marker regression\n");
            goto done;
        }
        if (fclose(marker) != 0) {
            fprintf(stderr, "unable to close truncated commit marker\n");
            goto done;
        }
        UDB_open(db);
        if (UDB_isOpen(db)) {
            fprintf(stderr, "database opened with an empty marker and committed data\n");
            goto done;
        }

        marker = fopen(marker_path, "w");
        if (NULL == marker || fputc(0, marker) == EOF) {
            if (marker) fclose(marker);
            fprintf(stderr, "unable to restore commit marker after empty-marker check\n");
            goto done;
        }
        if (fclose(marker) != 0) {
            fprintf(stderr, "unable to close final restored commit marker\n");
            goto done;
        }
        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
            !has_value(db, reused_pid, "reused-before-first-write")) {
            fprintf(stderr, "database data was damaged by marker-integrity checks\n");
            goto done;
        }
    }

    /* A symlinked commit marker must not redirect writes into its target. */
    {
        FILE *sentinel_file, *marker_file;
        char sentinel_path[160];
        char sentinel_contents[32];
        int sentinel_path_len = snprintf(sentinel_path, sizeof(sentinel_path),
            "%s.sentinel", path);

        if (sentinel_path_len < 0 ||
            (size_t)sentinel_path_len >= sizeof(sentinel_path)) {
            fprintf(stderr, "unable to construct marker sentinel path\n");
            goto done;
        }
        sentinel_file = fopen(sentinel_path, "w");
        if (NULL == sentinel_file) {
            fprintf(stderr, "unable to create marker sentinel\n");
            goto done;
        }
        if (fputs("untouched", sentinel_file) < 0) {
            fclose(sentinel_file);
            fprintf(stderr, "unable to write marker sentinel\n");
            remove(sentinel_path);
            goto done;
        }
        if (fclose(sentinel_file) != 0) {
            fprintf(stderr, "unable to close marker sentinel after writing\n");
            remove(sentinel_path);
            goto done;
        }

        UDB_close(db);
        if (remove(marker_path) != 0 ||
            symlink(sentinel_path, marker_path) != 0) {
            fprintf(stderr, "unable to install symlinked commit marker\n");
            remove(sentinel_path);
            goto done;
        }
        UDB_open(db);
        if (UDB_isOpen(db)) {
            fprintf(stderr, "database opened with a symlinked commit marker\n");
            remove(sentinel_path);
            goto done;
        }

        sentinel_file = fopen(sentinel_path, "r");
        if (NULL == sentinel_file ||
            NULL == fgets(sentinel_contents, sizeof(sentinel_contents), sentinel_file) ||
            strcmp(sentinel_contents, "untouched") != 0) {
            if (sentinel_file) fclose(sentinel_file);
            fprintf(stderr, "symlinked marker changed its target\n");
            remove(sentinel_path);
            goto done;
        }
        if (fclose(sentinel_file) != 0) {
            fprintf(stderr, "unable to close marker sentinel\n");
            remove(sentinel_path);
            goto done;
        }

        if (remove(marker_path) != 0) {
            fprintf(stderr, "unable to remove symlinked commit marker\n");
            remove(sentinel_path);
            goto done;
        }
        marker_file = fopen(marker_path, "w");
        if (NULL == marker_file || fputc(0, marker_file) == EOF) {
            if (marker_file) fclose(marker_file);
            fprintf(stderr, "unable to restore commit marker after symlink check\n");
            remove(sentinel_path);
            goto done;
        }
        if (fclose(marker_file) != 0) {
            fprintf(stderr, "unable to close restored marker after symlink check\n");
            remove(sentinel_path);
            goto done;
        }
        UDB_open(db);
        if (!UDB_isOpen(db) || !has_value(db, first_pid, "updated") ||
            !has_value(db, reused_pid, "reused-before-first-write")) {
            fprintf(stderr, "data was damaged by symlinked-marker rejection\n");
            remove(sentinel_path);
            goto done;
        }
        remove(sentinel_path);
    }

    result = EXIT_SUCCESS;

done:
    UDB_delete(db);
    UDB_free(db);
    return result;
}
