#include "UArray.h"

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
	UArray *path = NULL;
	UArray *component = NULL;
	UArray *extension = NULL;
	UArray *file_name = NULL;
	UArray *no_extension = NULL;
	UArray *empty_extension = NULL;

	UArray_appendPath_(NULL, NULL);
	UArray_removeLastPathComponent(NULL);
	UArray_clipBeforeLastPathComponent(NULL);
	UArray_removePathExtension(NULL);
	if (!check(UArray_findLastPathComponent(NULL) == -1,
		"NULL path component lookup must return -1") ||
		!check(UArray_lastPathComponent(NULL) == NULL,
		"NULL last path component must return NULL") ||
		!check(UArray_findPathExtension(NULL) == -1,
		"NULL path extension lookup must return -1") ||
		!check(UArray_pathExtension(NULL) == NULL,
		"NULL path extension must return NULL") ||
		!check(UArray_fileName(NULL) == NULL,
		"NULL file name must return NULL") ||
		!check(UArray_asOSPath(NULL) == NULL,
		"NULL OS path conversion must return NULL") ||
		!check(UArray_asUnixPath(NULL) == NULL,
		"NULL Unix path conversion must return NULL"))
		return EXIT_FAILURE;

	path = UArray_newWithCString_("/tmp/alpha.tar.gz");
	if (!check(path != NULL, "path fixture allocation failed"))
		goto failure;
	if (!check(UArray_findLastPathComponent(path) == 5,
		"last path component offset changed"))
		goto failure;
	component = UArray_lastPathComponent(path);
	extension = UArray_pathExtension(path);
	file_name = UArray_fileName(path);
	if (!check(component != NULL && strcmp(UArray_asCString(component), "alpha.tar.gz") == 0,
		"last path component extraction failed") ||
		!check(extension != NULL && strcmp(UArray_asCString(extension), "gz") == 0,
		"path extension extraction failed") ||
		!check(file_name != NULL && strcmp(UArray_asCString(file_name), "alpha.tar") == 0,
		"file name extraction failed"))
		goto failure;

	no_extension = UArray_newWithCString_("/tmp/README");
	if (!check(no_extension != NULL, "extension-free path fixture allocation failed"))
		goto failure;
	empty_extension = UArray_pathExtension(no_extension);
	if (!check(empty_extension != NULL && UArray_size(empty_extension) == 0,
		"extension-free path must still return an empty array"))
		goto failure;

	UArray_free(path);
	UArray_free(component);
	UArray_free(extension);
	UArray_free(file_name);
	UArray_free(no_extension);
	UArray_free(empty_extension);
	puts("UArray path null-safety test passed");
	return EXIT_SUCCESS;

failure:
	UArray_free(path);
	UArray_free(component);
	UArray_free(extension);
	UArray_free(file_name);
	UArray_free(no_extension);
	UArray_free(empty_extension);
	return EXIT_FAILURE;
}
