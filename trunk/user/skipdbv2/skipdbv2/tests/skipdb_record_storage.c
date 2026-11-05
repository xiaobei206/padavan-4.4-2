#include "SkipDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int datum_matches(Datum datum, const unsigned char *bytes, size_t length)
{
    return datum.size == length && (length == 0 ||
        (datum.data != NULL && memcmp(datum.data, bytes, length) == 0));
}

static int rejects_stream(const unsigned char *bytes, size_t length)
{
    BStream *reader = BStream_new();
    SkipDBRecord *record = SkipDBRecord_new();
    int rejected;

    if (NULL == reader || NULL == record) {
        BStream_free(reader);
        SkipDBRecord_dealloc(record);
        return 0;
    }
    BStream_setData_length_(reader, (unsigned char *)bytes, length);
    rejected = !SkipDBRecord_fromStream_(record, reader);
    BStream_free(reader);
    SkipDBRecord_dealloc(record);
    return rejected;
}

int main(void)
{
    static const unsigned char key[] = { 0x41, 0x00, 0xff, 0x42, 0x00 };
    static const unsigned char value[] = { 0x00, 0x7f, 0x80, 0xfe, 0x00, 0x31 };
    SkipDBRecord *source = NULL;
    SkipDBRecord *restored = NULL;
    BStream *writer = NULL;
    BStream *reader = NULL;
    UArray *encoded;
    const unsigned char *encoded_bytes;
    unsigned char *with_tail = NULL;
    size_t encoded_length;
    int result = EXIT_FAILURE;

    source = SkipDBRecord_new();
    restored = SkipDBRecord_new();
    writer = BStream_new();
    reader = BStream_new();
    if (NULL == source || NULL == restored || NULL == writer || NULL == reader) {
        fprintf(stderr, "record serialization fixture allocation failed\n");
        goto done;
    }

    SkipDBRecord_keyDatum_(source, Datum_FromData_length_((unsigned char *)key, sizeof(key)));
    SkipDBRecord_valueDatum_(source, Datum_FromData_length_((unsigned char *)value, sizeof(value)));
    SkipDBRecord_level_(source, 2);
    SkipDBRecord_atLevel_setPid_(source, 0, 17);
    SkipDBRecord_atLevel_setPid_(source, 1, 29);
    source->pointers[0].matchingPrefixSize = 3;
    source->pointers[1].matchingPrefixSize = 1;
    if (!SkipDBRecord_toStream_(source, writer)) {
        fprintf(stderr, "record serialization failed\n");
        goto done;
    }

    encoded = BStream_byteArray(writer);
    encoded_length = UArray_size(encoded);
    encoded_bytes = UArray_bytes(encoded);
    if (encoded_length == 0 || NULL == encoded_bytes) {
        fprintf(stderr, "record serializer produced an empty stream\n");
        goto done;
    }

    BStream_setData_length_(reader, (unsigned char *)encoded_bytes, encoded_length);
    if (!SkipDBRecord_fromStream_(restored, reader) ||
        !datum_matches(SkipDBRecord_keyDatum(restored), key, sizeof(key)) ||
        !datum_matches(SkipDBRecord_valueDatum(restored), value, sizeof(value)) ||
        SkipDBRecord_level(restored) != 2 ||
        SkipDBRecord_pidAtLevel_(restored, 0) != 17 ||
        SkipDBRecord_pidAtLevel_(restored, 1) != 29 ||
        restored->pointers[0].matchingPrefixSize != 3 ||
        restored->pointers[1].matchingPrefixSize != 1) {
        fprintf(stderr, "record round-trip changed bytes or skip-list links\n");
        goto done;
    }

    source->level = SKIPDB_MAX_LEVEL + 1;
    if (SkipDBRecord_recordAtLevel_(source, 0) != NULL) {
        fprintf(stderr, "invalid level was accepted by record link lookup\n");
        goto done;
    }
    source->level = -1;
    if (SkipDBRecord_recordAtLevel_(source, 0) != NULL) {
        fprintf(stderr, "negative level was accepted by record link lookup\n");
        goto done;
    }
    source->level = 2;

    if (!rejects_stream(encoded_bytes, encoded_length - 1)) {
        fprintf(stderr, "truncated record stream accepted\n");
        goto done;
    }

    with_tail = (unsigned char *)malloc(encoded_length + 1);
    if (NULL == with_tail) {
        fprintf(stderr, "unable to allocate trailing-data fixture\n");
        goto done;
    }
    memcpy(with_tail, encoded_bytes, encoded_length);
    with_tail[encoded_length] = 0xa5;
    if (!rejects_stream(with_tail, encoded_length + 1)) {
        fprintf(stderr, "stream trailing data accepted\n");
        goto done;
    }

    with_tail[0] = 0xff;
    if (!rejects_stream(with_tail, encoded_length)) {
        fprintf(stderr, "invalid record tag accepted\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("skipdb record storage test passed");

done:
    free(with_tail);
    BStream_free(reader);
    BStream_free(writer);
    SkipDBRecord_dealloc(restored);
    SkipDBRecord_dealloc(source);
    return result;
}
