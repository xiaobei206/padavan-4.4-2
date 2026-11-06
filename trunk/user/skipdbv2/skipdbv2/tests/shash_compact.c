#include "SHash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int keys_equal(void *left, void *right)
{
	return left != NULL && right != NULL && *(int *)left == *(int *)right;
}

static intptr_t hash_key(void *key)
{
	return key ? (intptr_t)*(int *)key : 0;
}

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "%s\n", message);
	return 0;
}

static int shashGrowRejectsInvalidState(int corruption)
{
	SHash *hash = SHash_new();
	SHashRecord *originalRecords;
	unsigned int originalTableSize, originalLog2, originalMask, originalNumKeys;
	int unchanged;

	if (NULL == hash)
		return 0;
	SHash_setKeysEqualCallback(hash, keys_equal);
	SHash_setHashForKeyCallback(hash, hash_key);
	originalRecords = hash->records;
	originalTableSize = hash->tableSize;
	originalLog2 = hash->log2tableSize;
	originalMask = hash->mask;
	originalNumKeys = hash->numKeys;

	switch (corruption) {
		case 0:
			hash->tableSize = 1; /* backing allocation remains sufficient for this empty fixture */
			break;
		case 1:
			hash->mask ^= 1u;
			break;
		case 2:
			hash->numKeys = hash->tableSize * 2 + 1;
			break;
		default:
			SHash_free(hash);
			return 0;
	}

	{
		unsigned int malformedTableSize = hash->tableSize;
		unsigned int malformedMask = hash->mask;
		unsigned int malformedNumKeys = hash->numKeys;
		SHash_grow(hash);
		unchanged = hash->records == originalRecords &&
			hash->tableSize == malformedTableSize &&
			hash->log2tableSize == originalLog2 &&
			hash->mask == malformedMask &&
			hash->numKeys == malformedNumKeys;
	}
	SHash_free(hash);
	return unchanged && originalTableSize == 2 && originalMask == 1 &&
		originalNumKeys == 0;
}


static intptr_t constant_hash_key(void *key)
{
	(void)key;
	return 0;
}

static int shashCuckooFailurePreservesState(void)
{
	SHash *hash = SHash_new();
	int keys[3] = {11, 22, 33};
	int values[3] = {101, 202, 303};
	SHashRecord originalRecords[4], incoming;
	SHashRecord *record;
	unsigned int originalBalance;
	int okay;

	if (NULL == hash)
		return 0;
	SHash_setKeysEqualCallback(hash, keys_equal);
	SHash_setHashForKeyCallback(hash, constant_hash_key);
	SHash_at_put_(hash, &keys[0], &values[0]);
	SHash_at_put_(hash, &keys[1], &values[1]);
	if (hash->tableSize != 2 || hash->numKeys != 2) {
		SHash_free(hash);
		return 0;
	}

	memcpy(originalRecords, hash->records, sizeof(originalRecords));
	originalBalance = hash->balance;
	incoming.key = &keys[2];
	incoming.value = &values[2];
	record = SHash_cuckoo_(hash, &incoming);
	okay = NULL == record &&
		incoming.key == &keys[2] && incoming.value == &values[2] &&
		hash->balance == originalBalance && hash->numKeys == 2 &&
		0 == memcmp(originalRecords, hash->records, sizeof(originalRecords)) &&
		SHash_at_(hash, &keys[0]) == &values[0] &&
		SHash_at_(hash, &keys[1]) == &values[1];
	SHash_free(hash);
	return okay;
}

int main(void)
{
	SHash *hash;
	int keys[12], values[12];
	unsigned int i, oldTableSize;
	size_t oldMemory;

	SHash_compact(NULL);

	if (!check(shashCuckooFailurePreservesState(),
		"SHash cuckoo failure altered the original table or input record"))
		return EXIT_FAILURE;
	if (!check(shashGrowRejectsInvalidState(0),
		"SHash_grow accepted inconsistent table geometry"))
		return EXIT_FAILURE;
	if (!check(shashGrowRejectsInvalidState(1),
		"SHash_grow accepted an inconsistent hash mask"))
		return EXIT_FAILURE;
	if (!check(shashGrowRejectsInvalidState(2),
		"SHash_grow accepted an impossible key count"))
		return EXIT_FAILURE;

	hash = SHash_new();
	if (!check(hash != NULL, "SHash allocation failed"))
		return EXIT_FAILURE;
	SHash_setKeysEqualCallback(hash, keys_equal);
	SHash_setHashForKeyCallback(hash, hash_key);

	for (i = 0; i < 12; ++i) {
		keys[i] = (int)i + 1;
		values[i] = (int)(i + 100);
		SHash_at_put_(hash, &keys[i], &values[i]);
	}
	if (!check(hash->numKeys == 12, "SHash inserts lost entries before compaction"))
		goto failure;
	oldTableSize = hash->tableSize;
	oldMemory = SHash_memorySize(hash);
	if (!check(oldTableSize > 2, "SHash did not grow enough for the compaction test"))
		goto failure;

	for (i = 2; i < 12; ++i)
		SHash_removeKey_(hash, &keys[i]);
	if (!check(hash->numKeys == 2 &&
		SHash_at_(hash, &keys[0]) == &values[0] &&
		SHash_at_(hash, &keys[1]) == &values[1],
		"SHash removals or retained values are incorrect"))
		goto failure;

	SHash_compact(hash);
	if (!check(hash->tableSize < oldTableSize &&
		hash->numKeys == 2 && SHash_memorySize(hash) < oldMemory,
		"SHash_compact did not reduce the table while preserving count"))
		goto failure;
	for (i = 0; i < 2; ++i) {
		if (!check(SHash_at_(hash, &keys[i]) == &values[i],
			"SHash_compact lost a retained key/value mapping"))
			goto failure;
	}

	SHash_at_put_(hash, &keys[2], &values[2]);
	if (!check(hash->numKeys == 3 &&
		SHash_at_(hash, &keys[2]) == &values[2],
		"SHash insertion failed after compaction"))
		goto failure;
	for (i = 0; i < 3; ++i)
		SHash_removeKey_(hash, &keys[i]);
	SHash_compact(hash);
	if (!check(hash->numKeys == 0 && hash->tableSize == 2,
		"SHash_compact did not return an empty table to its minimum size"))
		goto failure;

	SHash_free(hash);
	puts("SHash compact test passed");
	return EXIT_SUCCESS;

failure:
	SHash_free(hash);
	return EXIT_FAILURE;
}
