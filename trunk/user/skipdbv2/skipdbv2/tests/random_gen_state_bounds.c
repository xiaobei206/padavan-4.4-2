#include "RandomGen.h"

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

int main(void)
{
	RandomGen *actual = RandomGen_new();
	RandomGen *expected = RandomGen_new();
	int value;

	if (actual == NULL || expected == NULL) {
		RandomGen_free(actual);
		RandomGen_free(expected);
		fprintf(stderr, "RandomGen allocation failed\n");
		return EXIT_FAILURE;
	}

	RandomGen_setSeed(actual, 5489UL);
	RandomGen_setSeed(expected, 5489UL);
	actual->mti = -1;
	value = RandomGen_randomInt(actual);
	if (!check(value == RandomGen_randomInt(expected),
		"negative generator index did not recover to the default seed"))
		goto failure;

	RandomGen_setSeed(actual, 5489UL);
	RandomGen_setSeed(expected, 5489UL);
	actual->mti = RANDOMGEN_N + 2;
	if (!check(RandomGen_randomInt(actual) == RandomGen_randomInt(expected),
		"out-of-range generator index did not recover to the default seed"))
		goto failure;

	RandomGen_setSeed(actual, 5489UL);
	RandomGen_setSeed(expected, 5489UL);
	memset(actual->mt, 0, sizeof(actual->mt));
	actual->mti = 0;
	if (!check(RandomGen_gaussian(actual, 5.0, 2.0) ==
		RandomGen_gaussian(expected, 5.0, 2.0),
		"all-zero generator state did not recover before Gaussian sampling"))
		goto failure;

	RandomGen_free(actual);
	RandomGen_free(expected);
	puts("RandomGen index bounds test passed");
	return EXIT_SUCCESS;

failure:
	RandomGen_free(actual);
	RandomGen_free(expected);
	return EXIT_FAILURE;
}
