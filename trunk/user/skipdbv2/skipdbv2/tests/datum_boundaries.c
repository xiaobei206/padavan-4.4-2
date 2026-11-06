#include "Datum.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	unsigned char match;
	unsigned char stop;
	size_t calls;
	size_t matchedSuffixSize;
} DetectContext;

static int detectFirstByte_(void *target, Datum *candidate)
{
	DetectContext *context = (DetectContext *)target;
	if (NULL == context || NULL == candidate)
		return -1;
	context->calls++;
	if (0 == candidate->size || NULL == candidate->data)
		return 0;
	if (candidate->data[0] == context->match) {
		context->matchedSuffixSize = candidate->size;
		return 1;
	}
	if (context->stop && candidate->data[0] == context->stop)
		return -1;
	return 0;
}

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
	const unsigned char searchableBytes[] = { 'a', 'l', 'p', 'h', 'a' };
	Datum alpha = Datum_FromCString_("alpha");
	Datum alpine = Datum_FromCString_("alpine");
	Datum prefix = Datum_FromData_length_((unsigned char *)prefixBytes, sizeof(prefixBytes));
	Datum suffix = Datum_FromData_length_((unsigned char *)suffixBytes, sizeof(suffixBytes));
	Datum empty = Datum_Empty();
	Datum malformed = { 3, NULL };
	Datum malformedLarger = { 5, NULL };
	Datum searchable = Datum_FromData_length_((unsigned char *)searchableBytes, sizeof(searchableBytes));
	DetectContext detect = { 'h', 0, 0, 0 };
	Datum at;
	Datum *slice;


	if (!check(Datum_detect_with_(&searchable, detectFirstByte_, &detect) == 3 &&
		detect.calls == 4 && detect.matchedSuffixSize == 2,
		"Datum detection returned an incorrect match offset or suffix"))
		return EXIT_FAILURE;
	detect.match = 'z';
	detect.stop = 0;
	detect.calls = 0;
	detect.matchedSuffixSize = 0;
	if (!check(Datum_detect_with_(&searchable, detectFirstByte_, &detect) == -1 &&
		detect.calls == sizeof(searchableBytes),
		"Datum detection did not scan every unmatched byte"))
		return EXIT_FAILURE;
	detect.stop = 'p';
	detect.calls = 0;
	if (!check(Datum_detect_with_(&searchable, detectFirstByte_, &detect) == -1 &&
		detect.calls == 3,
		"Datum detection did not honor callback early stop"))
		return EXIT_FAILURE;
	detect.calls = 0;
	if (!check(Datum_detect_with_(&empty, detectFirstByte_, &detect) == -1 &&
		detect.calls == 0 &&
		Datum_detect_with_(NULL, detectFirstByte_, &detect) == -1 &&
		Datum_detect_with_(&malformed, detectFirstByte_, &detect) == -1 &&
		Datum_detect_with_(&searchable, NULL, &detect) == -1 &&
		detect.calls == 0,
		"Datum detection did not reject empty or invalid inputs before callbacks"))
		return EXIT_FAILURE;
	if ((size_t)INT_MAX < (size_t)-1) {
		Datum oversized = { (size_t)INT_MAX + 1u, (unsigned char *)searchableBytes };
		if (!check(Datum_detect_with_(&oversized, detectFirstByte_, &detect) == -1 &&
			detect.calls == 0,
			"Datum detection must reject unrepresentable offsets"))
			return EXIT_FAILURE;
	}

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
