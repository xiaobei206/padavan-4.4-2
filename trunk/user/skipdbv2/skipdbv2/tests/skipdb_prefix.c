#include "SkipDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *const *expected;
    size_t expected_count;
    size_t seen;
    int failed;
} PrefixCapture;

static int record_has_key(SkipDBRecord *record, const char *expected)
{
    Datum key;
    size_t length;

    if (NULL == record || NULL == expected)
        return 0;
    key = SkipDBRecord_keyDatum(record);
    length = strlen(expected) + 1;
    return key.data != NULL && key.size == length &&
        memcmp(key.data, expected, length) == 0;
}

static void capture_prefix_record(SkipDB *db, SkipDBRecord *record, void *context)
{
    PrefixCapture *capture = (PrefixCapture *)context;

    (void)db;
    if (NULL == capture || capture->failed)
        return;
    if (capture->seen >= capture->expected_count ||
        !record_has_key(record, capture->expected[capture->seen])) {
        capture->failed = 1;
        return;
    }
    capture->seen++;
}

static int callback_matches(SkipDB *db, Datum prefix,
    const char *const *expected, size_t expected_count)
{
    PrefixCapture capture;

    capture.expected = expected;
    capture.expected_count = expected_count;
    capture.seen = 0;
    capture.failed = 0;
    SkipDB_list_prefix(db, prefix, &capture, capture_prefix_record);
    return !capture.failed && capture.seen == expected_count;
}

static int insert_key(SkipDB *db, const char *key)
{
    return SkipDB_at_put_(db, Datum_FromCString_(key),
        Datum_FromCString_(key));
}

static int fail(const char *message)
{
    fprintf(stderr, "skipdb_prefix_test: %s\n", message);
    return EXIT_FAILURE;
}

int main(void)
{
    static const char *const prefix_keys[] = {
        "al", "albatross", "alpha", "alphabet", "alpine"
    };
    static const char *const all_keys[] = {
        "al", "albatross", "alpha", "alphabet", "alpine", "beta", "gamma"
    };
    static const char *const inserted_keys[] = {
        "alpha", "alpine", "beta", "al", "alphabet", "albatross", "gamma"
    };
    SkipDB *db = NULL;
    SkipDBCursor *cursor = NULL;
    SkipDBRecord *record;
    Datum prefix = Datum_FromCString_("al");
    Datum empty_prefix = Datum_FromCString_("");
    Datum missing_prefix = Datum_FromCString_("zz");
    char path[128];
    int path_length;
    size_t i;
    int result = EXIT_FAILURE;

    db = SkipDB_new();
    if (NULL == db)
        return fail("database allocation failed");
    path_length = snprintf(path, sizeof(path), "skipdb_prefix_test_%ld",
        (long)getpid());
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        result = fail("test path overflow");
        goto done;
    }

    SkipDB_setPath_(db, path);
    SkipDB_delete(db);
    if (!SkipDB_open(db) || !SkipDB_isOpen(db)) {
        result = fail("database open failed");
        goto done;
    }

    SkipDB_beginTransaction(db);
    for (i = 0; i < sizeof(inserted_keys) / sizeof(inserted_keys[0]); ++i) {
        if (!SkipDB_at_put_(db, Datum_FromCString_(inserted_keys[i]),
            Datum_FromCString_(inserted_keys[i])) || !SkipDB_isOpen(db)) {
            result = fail("fixture insertion failed");
            goto done;
        }
    }
    SkipDB_commitTransaction(db);
    if (!SkipDB_isOpen(db) || SkipDB_count(db) != 7) {
        result = fail("fixture commit/count failed");
        goto done;
    }

    record = SkipDB_list_first(db, prefix, &cursor);
    if (NULL == cursor) {
        result = fail("prefix cursor was not created");
        goto done;
    }
    for (i = 0; i < sizeof(prefix_keys) / sizeof(prefix_keys[0]); ++i) {
        if (!record_has_key(record, prefix_keys[i])) {
            result = fail("cursor prefix order or boundary mismatch");
            goto done;
        }
        record = SkipDB_list_next(db, prefix, cursor);
    }
    if (record != NULL) {
        result = fail("cursor prefix scan crossed its boundary");
        goto done;
    }
    SkipDBCursor_release(cursor);
    cursor = NULL;

    if (!callback_matches(db, prefix, prefix_keys,
        sizeof(prefix_keys) / sizeof(prefix_keys[0]))) {
        result = fail("callback prefix scan mismatch");
        goto done;
    }
    if (!callback_matches(db, empty_prefix, all_keys,
        sizeof(all_keys) / sizeof(all_keys[0]))) {
        result = fail("empty-string prefix did not scan all records");
        goto done;
    }

    record = SkipDB_list_first(db, missing_prefix, &cursor);
    if (record != NULL || NULL == cursor) {
        result = fail("missing prefix did not return an empty cursor result");
        goto done;
    }
    SkipDBCursor_release(cursor);
    cursor = NULL;

    result = EXIT_SUCCESS;
    puts("SkipDB prefix query tests passed");

done:
    if (cursor)
        SkipDBCursor_release(cursor);
    SkipDB_delete(db);
    SkipDB_free(db);
    return result;
}
