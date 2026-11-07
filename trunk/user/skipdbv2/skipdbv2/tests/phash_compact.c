#include "PHash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "%s\n", message);
	return 0;
}

static int phashGrowRejectsInvalidState(int corruption)
{
	PHash *hash = PHash_new();
	PHashRecord *originalRecords;
	unsigned int originalTableSize, originalLog2, originalMask, originalNumKeys;
	int unchanged;

	if (NULL == hash)
		return 0;
	originalRecords = hash->records;
	originalTableSize = hash->tableSize;
	originalLog2 = hash->log2tableSize;
	originalMask = hash->mask;
	originalNumKeys = hash->numKeys;

	switch (corruption) {
		case 0:
			hash->tableSize = 1; /* allocation is still large enough for this safe fixture */
			break;
		case 1:
			hash->mask ^= 1u;
			break;
		case 2:
			hash->numKeys = hash->tableSize * 2 + 1;
			break;
		default:
			PHash_free(hash);
			return 0;
	}

	{
		unsigned int malformedTableSize = hash->tableSize;
		unsigned int malformedMask = hash->mask;
		unsigned int malformedNumKeys = hash->numKeys;
		PHash_grow(hash);
		unchanged = hash->records == originalRecords &&
			hash->tableSize == malformedTableSize &&
			hash->log2tableSize == originalLog2 &&
			hash->mask == malformedMask &&
			hash->numKeys == malformedNumKeys;
	}
	PHash_free(hash);
	return unchanged && originalTableSize == 2 && originalMask == 1 &&
		originalNumKeys == 0;
}


static int phashCuckooFailurePreservesState(void)
{
	PHash *hash = PHash_new();
	int keys[64], values[3] = {101, 202, 303};
	void *keysByPair[4][3] = {{NULL}};
	unsigned int counts[4] = {0, 0, 0, 0};
	PHashRecord originalRecords[4], incoming;
	PHashRecord *record;
	unsigned int originalBalance, i, pair, selectedPair;
	int okay;

	if (NULL == hash)
		return 0;
	selectedPair = 4;
	for (i = 0; i < 64; ++i) {
		unsigned int primaryHash, primaryIndex, alternateIndex;
		keys[i] = (int)i + 1;
		primaryHash = PHash_hash(hash, &keys[i]);
		primaryIndex = primaryHash & hash->mask;
		alternateIndex = PHash_hash_more(hash, primaryHash) & hash->mask;
		pair = primaryIndex | (alternateIndex << 1);
		if (counts[pair] < 3)
			keysByPair[pair][counts[pair]++] = &keys[i];
	}
	for (pair = 0; pair < 4; ++pair) {
		if (counts[pair] == 3) {
			selectedPair = pair;
			break;
		}
	}
	if (selectedPair == 4) {
		PHash_free(hash);
		return 0;
	}

	PHash_at_put_(hash, keysByPair[selectedPair][0], &values[0]);
	PHash_at_put_(hash, keysByPair[selectedPair][1], &values[1]);
	if (hash->tableSize != 2 || hash->numKeys != 2) {
		PHash_free(hash);
		return 0;
	}

	memcpy(originalRecords, hash->records, sizeof(originalRecords));
	originalBalance = hash->balance;
	incoming.key = keysByPair[selectedPair][2];
	incoming.value = &values[2];
	record = PHash_cuckoo_(hash, &incoming);
	okay = NULL == record &&
		incoming.key == keysByPair[selectedPair][2] &&
		incoming.value == &values[2] &&
		hash->balance == originalBalance && hash->numKeys == 2 &&
		0 == memcmp(originalRecords, hash->records, sizeof(originalRecords)) &&
		PHash_at_(hash, keysByPair[selectedPair][0]) == &values[0] &&
		PHash_at_(hash, keysByPair[selectedPair][1]) == &values[1];
	PHash_free(hash);
	return okay;
}


static int phashFullHashCollisionDoesNotGrow(void)
{
#if UINTPTR_MAX > UINT32_MAX
	PHash *hash = PHash_new();
	int values[3] = {101, 202, 303};
	uintptr_t base = (uintptr_t)&values[0];
	void *keys[3];
	PHashRecord *originalRecords;
	unsigned int originalBalance;
	int okay = 0;

	if (NULL == hash)
		return 0;
	/* PHash_hash retains only the low 32 bits of pointer ^ (pointer >> 4).
	 * Bits 36 and 37 therefore produce distinct values with the same hash. */
	keys[0] = (void *)base;
	keys[1] = (void *)(base ^ ((uintptr_t)1 << 36));
	keys[2] = (void *)(base ^ ((uintptr_t)1 << 37));
	if (PHash_hash(hash, keys[0]) != PHash_hash(hash, keys[1]) ||
		PHash_hash(hash, keys[0]) != PHash_hash(hash, keys[2]))
		goto done;

	PHash_at_put_(hash, keys[0], &values[0]);
	PHash_at_put_(hash, keys[1], &values[1]);
	if (hash->tableSize != 2 || hash->numKeys != 2)
		goto done;

	originalRecords = hash->records;
	originalBalance = hash->balance;
	PHash_at_put_(hash, keys[2], &values[2]);
	okay = hash->records == originalRecords &&
		hash->tableSize == 2 && hash->numKeys == 2 &&
		hash->balance == originalBalance &&
		PHash_at_(hash, keys[0]) == &values[0] &&
		PHash_at_(hash, keys[1]) == &values[1] &&
		PHash_at_(hash, keys[2]) == NULL;

done:
	PHash_free(hash);
	return okay;
#else
	/* The 32-bit pointer transform is injective; no such collision exists. */
	return 1;
#endif
}


static int phashOperationsRejectInvalidGeometry(int corruption)
{
	PHash *hash = NULL, *destination = NULL, *clone = NULL;
	int key = 101, value = 202, otherKey = 303, otherValue = 404;
	int replacement = 505;
	PHashRecord originalRecords[4], destinationRecords[4], incoming;
	unsigned int savedTableSize = 0, savedLog2 = 0, savedMask = 0;
	unsigned int savedNumKeys = 0, savedBalance = 0, iterationCount = 0;
	void *savedNullValue = NULL;
	int savedState = 0, okay = 0;

	hash = PHash_new();
	destination = PHash_new();
	if (NULL == hash || NULL == destination)
		goto done;
	PHash_at_put_(hash, &key, &value);
	PHash_at_put_(destination, &otherKey, &otherValue);
	if (hash->tableSize != 2 || hash->numKeys != 1 ||
		destination->tableSize != 2 || destination->numKeys != 1)
		goto done;

	memcpy(originalRecords, hash->records, sizeof(originalRecords));
	memcpy(destinationRecords, destination->records, sizeof(destinationRecords));
	savedTableSize = hash->tableSize;
	savedLog2 = hash->log2tableSize;
	savedMask = hash->mask;
	savedNumKeys = hash->numKeys;
	savedBalance = hash->balance;
	savedNullValue = hash->nullRecord.value;
	savedState = 1;
	hash->nullRecord.value = &replacement;

	switch (corruption) {
		case 0:
			hash->tableSize = 1;
			break;
		case 1:
			hash->mask ^= 1u;
			break;
		case 2:
			hash->numKeys = hash->tableSize * 2 + 1;
			break;
		case 3:
			hash->log2tableSize = 0;
			break;
		case 4:
			/* The real allocation stays four records despite this claim. */
			hash->tableSize = 1u << 20;
			break;
		default:
			goto done;
	}

	if (PHash_hasValidStorage_(hash) ||
		PHash_count(hash) != 0 ||
		PHash_memorySize(hash) != 0 ||
		PHash_recordAt_(hash, &otherKey) != &hash->nullRecord ||
		PHash_at_(hash, &otherKey) != NULL ||
		PHash_at_update_(hash, &key, &replacement) != 0)
		goto done;

	clone = PHash_clone(hash);
	if (clone)
		goto done;
	incoming.key = &otherKey;
	incoming.value = &otherValue;
	if (PHash_cuckoo_(hash, &incoming) != NULL ||
		incoming.key != &otherKey || incoming.value != &otherValue)
		goto done;

	PHash_at_put_(hash, &otherKey, &otherValue);
	PHash_removeKey_(hash, &key);
	PHash_removeValue_(hash, &value);
	PHash_clean(hash);
	PHash_compact(hash);
	PHash_grow(hash);
	if (PHash_firstKeyForValue_(hash, &value) != NULL)
		goto done;

	PHash_copy_(destination, hash);
	PHash_copy_(hash, destination);
	PHASH_FOREACH(hash, iterKey, iterValue, ++iterationCount);

	if (iterationCount != 0 ||
		0 != memcmp(originalRecords, hash->records, sizeof(originalRecords)) ||
		0 != memcmp(destinationRecords, destination->records, sizeof(destinationRecords)) ||
		destination->numKeys != 1 ||
		PHash_at_(destination, &otherKey) != &otherValue)
		goto done;

	okay = 1;

done:
	if (savedState && hash) {
		hash->tableSize = savedTableSize;
		hash->log2tableSize = savedLog2;
		hash->mask = savedMask;
		hash->numKeys = savedNumKeys;
		hash->balance = savedBalance;
		hash->nullRecord.value = savedNullValue;
	}
	PHash_free(clone);
	PHash_free(destination);
	PHash_free(hash);
	return okay;
}

static int phashCopyPreservesSharedBacking(void)
{
	PHash *source = PHash_new();
	PHash alias = {0};
	int key = 707;
	int value = 808;
	int okay = 0;

	if (NULL == source)
		return 0;
	PHash_at_put_(source, &key, &value);
	if (source->numKeys != 1 || PHash_at_(source, &key) != &value)
		goto done;

	/* Copying into a shallow struct alias must not free source->records. */
	alias = *source;
	PHash_copy_(&alias, source);
	if (alias.records == source->records ||
		PHash_at_(source, &key) != &value ||
		PHash_at_(&alias, &key) != &value)
		goto done;
	okay = 1;

done:
	if (alias.records && alias.records != source->records)
		io_free(alias.records);
	PHash_free(source);
	return okay;
}


static int phashRejectsMisalignedStorage(void)
{
	PHash *source = PHash_new();
	PHash malformed;
	PHashRecord *originalRecords;
	int key = 707, value = 808;
	unsigned int originalTableSize, originalLog2, originalMask;
	unsigned int originalNumKeys, originalBalance;
	int okay = 0;

	if (NULL == source)
		return 0;
	PHash_at_put_(source, &key, &value);
	if (source->numKeys != 1 || PHash_at_(source, &key) != &value)
		goto done;

	originalRecords = source->records;
	originalTableSize = source->tableSize;
	originalLog2 = source->log2tableSize;
	originalMask = source->mask;
	originalNumKeys = source->numKeys;
	originalBalance = source->balance;

	malformed = *source;
	malformed.records = (PHashRecord *)((uintptr_t)source->records + 1u);
	if (PHash_hasValidStorage_(&malformed)) {
		fprintf(stderr, "PHash accepted misaligned backing records storage\n");
		goto done;
	}

	/* All invalid-storage operations must now fail closed without dereferencing records. */
	if (PHash_count(&malformed) != 0 ||
		PHash_memorySize(&malformed) != 0 ||
		PHash_recordAt_(&malformed, &key) != &malformed.nullRecord ||
		PHash_at_(&malformed, &key) != NULL ||
		PHash_at_update_(&malformed, &key, &value) != 0)
		goto done;
	PHash_clean(&malformed);
	PHash_at_put_(&malformed, &key, &value);
	PHash_removeKey_(&malformed, &key);
	PHash_removeValue_(&malformed, &value);
	PHash_compact(&malformed);
	PHash_grow(&malformed);
	PHash_copy_(&malformed, source);
	PHASH_FOREACH(&malformed, iterKey, iterValue, (void)iterKey; (void)iterValue;);

	if (malformed.records != (PHashRecord *)((uintptr_t)originalRecords + 1u) ||
		malformed.tableSize != originalTableSize ||
		malformed.log2tableSize != originalLog2 ||
		malformed.mask != originalMask ||
		malformed.numKeys != originalNumKeys ||
		malformed.balance != originalBalance ||
		source->records != originalRecords ||
		source->tableSize != originalTableSize ||
		source->numKeys != originalNumKeys ||
		PHash_at_(source, &key) != &value)
		goto done;

	okay = 1;
done:
	PHash_free(source);
	return okay;
}


int main(void)
{
	PHash *hash;
	int keys[12], values[12];
	unsigned int i, oldTableSize;
	size_t oldMemory;

	PHash_compact(NULL);

	if (!check(phashRejectsMisalignedStorage(),
		"PHash operations accepted misaligned backing storage"))
		return EXIT_FAILURE;
	if (!check(phashCuckooFailurePreservesState(),
		"PHash cuckoo failure altered the original table or input record"))
		return EXIT_FAILURE;
	if (!check(phashFullHashCollisionDoesNotGrow(),
		"PHash full-hash collisions triggered futile table growth"))
		return EXIT_FAILURE;
	if (!check(phashCopyPreservesSharedBacking(),
		"PHash shallow aliases must not free the source backing allocation"))
		return EXIT_FAILURE;
	if (!check(phashGrowRejectsInvalidState(0),
		"PHash_grow accepted inconsistent table geometry"))
		return EXIT_FAILURE;
	if (!check(phashGrowRejectsInvalidState(1),
		"PHash_grow accepted an inconsistent hash mask"))
		return EXIT_FAILURE;
	if (!check(phashGrowRejectsInvalidState(2),
		"PHash_grow accepted an impossible key count"))
		return EXIT_FAILURE;
	for (i = 0; i < 5; ++i) {
		if (!check(phashOperationsRejectInvalidGeometry((int)i),
			"PHash operations accessed an invalid table geometry"))
			return EXIT_FAILURE;
	}

	hash = PHash_new();
	if (!check(hash != NULL, "PHash allocation failed"))
		return EXIT_FAILURE;

	for (i = 0; i < 12; ++i) {
		keys[i] = (int)i + 1;
		values[i] = (int)(i + 100);
		PHash_at_put_(hash, &keys[i], &values[i]);
	}
	if (!check(hash->numKeys == 12, "PHash inserts lost entries before compaction"))
		goto failure;
	oldTableSize = hash->tableSize;
	oldMemory = PHash_memorySize(hash);
	if (!check(oldTableSize > 2, "PHash did not grow enough for the compaction test"))
		goto failure;

	for (i = 0; i < 12; ++i) {
		if (i != 0 && i != 4)
			PHash_removeKey_(hash, &keys[i]);
	}
	if (!check(hash->numKeys == 2 &&
		PHash_at_(hash, &keys[0]) == &values[0] &&
		PHash_at_(hash, &keys[4]) == &values[4],
		"PHash removals or retained values are incorrect"))
		goto failure;

	PHash_compact(hash);
	if (!check(hash->tableSize < oldTableSize &&
		hash->numKeys == 2 && PHash_memorySize(hash) < oldMemory,
		"PHash_compact did not reduce the table while preserving count"))
		goto failure;
	if (!check(PHash_at_(hash, &keys[0]) == &values[0] &&
		PHash_at_(hash, &keys[4]) == &values[4],
		"PHash_compact lost a retained key/value mapping"))
		goto failure;

	PHash_at_put_(hash, &keys[1], &values[1]);
	if (!check(hash->numKeys == 3 &&
		PHash_at_(hash, &keys[1]) == &values[1],
		"PHash insertion failed after compaction"))
		goto failure;
	PHash_removeKey_(hash, &keys[0]);
	PHash_removeKey_(hash, &keys[1]);
	PHash_removeKey_(hash, &keys[4]);
	PHash_compact(hash);
	if (!check(hash->numKeys == 0 && hash->tableSize == 2,
		"PHash_compact did not return an empty table to its minimum size"))
		goto failure;

	PHash_free(hash);
	puts("PHash compact test passed");
	return EXIT_SUCCESS;

failure:
	PHash_free(hash);
	return EXIT_FAILURE;
}
