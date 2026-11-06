#include "Datum.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "%s\n", message);
	return 0;
}

int main(void)
{
	const unsigned char prefixBytes[] = { 'a', 'l', 'p' };
	const unsigned char suffixBytes[] = { 'a', '\0' };
	Datum alpha = Datum_FromCString_("alpha");
	Datum alpine = Datum_FromCString_("alpine");
	Datum prefix = Datum_FromData_length_((unsigned char *)prefixBytes, sizeof(prefixBytes));
	Datum suffix = Datum_FromData_length_((unsigned char *)suffixBytes, sizeof(suffixBytes));
	Datum empty = Datum_Empty();
	Datum malformed = { 3, NULL };
	Datum malformedLarger = { 5, NULL };
	Datum at;
	Datum *slice;

	if (!check(Datum_compare_(&alpha, &alpine) < 0 &&
		Datum_compare_(&alpine, &alpha) > 0,
		"valid Datum comparison lost ordering"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_length_(&alpha, &alpine, 2) == 0 &&
		Datum_compare_length_(&alpha, &alpine, 4) < 0,
		"valid Datum prefix comparison changed"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_(&malformed, &alpha) < 0 &&
		Datum_compare_(&alpha, &malformed) > 0,
		"malformed Datum comparison must be antisymmetric"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_(&malformed, &malformedLarger) < 0 &&
		Datum_compare_(&malformedLarger, &malformed) > 0,
		"malformed Datum comparison must order invalid sizes consistently"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_(&malformed, &malformed) == 0 &&
		Datum_compare_length_(&malformed, &malformed, 4) == 0,
		"identical invalid Datum comparison must be stable"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_length_(&malformed, &alpha, 4) < 0 &&
		Datum_compare_length_(&alpha, &malformed, 4) > 0,
		"malformed prefix comparison must be antisymmetric"))
		return EXIT_FAILURE;
	if (!check(Datum_compare_(NULL, &alpha) < 0 &&
		Datum_compare_(&alpha, NULL) > 0 &&
		Datum_compare_(NULL, NULL) == 0,
		"NULL Datum comparison must be stable"))
		return EXIT_FAILURE;

	if (!check(Datum_beginsWith_(&malformed, &malformed) == 0 &&
		Datum_endsWith_(&malformed, &malformed) == 0,
		"malformed Datum must not match its own prefix or suffix"))
		return EXIT_FAILURE;
	if (!check(Datum_beginsWith_(&alpha, &prefix) &&
		Datum_endsWith_(&alpha, &suffix) &&
		Datum_beginsWith_(&alpha, &empty) &&
		Datum_endsWith_(&alpha, &empty),
		"valid prefix/suffix boundary behavior changed"))
		return EXIT_FAILURE;
	if (!check(Datum_hash(&malformed) == 0 &&
		Datum_matchingPrefixSizeWith_(&malformed, &alpha) == 0 &&
		Datum_asUArray(&malformed) == NULL,
		"malformed Datum must be rejected by data accessors"))
		return EXIT_FAILURE;
	at = Datum_datumAt_(&malformed, 0);
	if (!check(at.size == 0 && at.data == NULL,
		"malformed Datum indexing must return an empty Datum"))
		return EXIT_FAILURE;
	if (!check(Datum_newFrom_to_(&malformed, 0, 1) == NULL,
		"malformed Datum slicing must fail"))
		return EXIT_FAILURE;
	slice = Datum_newFrom_to_(&alpha, 0, 3);
	if (!check(slice != NULL && slice->size == 3 &&
		Datum_compare_(slice, &prefix) == 0,
		"valid Datum slicing changed")) {
		free(slice);
		return EXIT_FAILURE;
	}
	free(slice);
	puts("Datum boundary test passed");
	return EXIT_SUCCESS;
}
