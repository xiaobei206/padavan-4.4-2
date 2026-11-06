#include "PHash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void)
{
	PHash *hash;
	int keys[12], values[12];
	unsigned int i, oldTableSize;
	size_t oldMemory;

	PHash_compact(NULL);

	if (!check(phashCuckooFailurePreservesState(),
		"PHash cuckoo failure altered the original table or input record"))
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
