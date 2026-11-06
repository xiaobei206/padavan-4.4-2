#include "UArray.h"

#include <stdio.h>
#include <limits.h>
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
	UArray *path_with_directory_dot = NULL;
	UArray *directory_dot_extension = NULL;
	UArray *directory_dot_file = NULL;
	UArray *dotfile_path = NULL;
	UArray *dotfile_extension = NULL;
	UArray *dotfile_file = NULL;
	UArray *trailing_path = NULL;
	UArray *trailing_component = NULL;
	UArray *trailing_extension = NULL;
	UArray *trailing_file_name = NULL;
	UArray *clip_path = NULL;
	UArray *root_path = NULL;
	UArray *root_component = NULL;
	UArray *malformed = NULL;
	UArray *safe_path = NULL;
	size_t malformed_size;

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

	path_with_directory_dot = UArray_newWithCString_("dir.with.dot/file");
	if (!check(path_with_directory_dot != NULL,
		"directory-dot path fixture allocation failed"))
		goto failure;
	directory_dot_extension = UArray_pathExtension(path_with_directory_dot);
	directory_dot_file = UArray_fileName(path_with_directory_dot);
	if (!check(UArray_findPathExtension(path_with_directory_dot) == -1 &&
		directory_dot_extension != NULL && UArray_size(directory_dot_extension) == 0,
		"directory dots must not produce path extension") ||
		!check(directory_dot_file != NULL &&
			strcmp(UArray_asCString(directory_dot_file), "file") == 0,
			"directory dot confused file-name extraction"))
		goto failure;

	dotfile_path = UArray_newWithCString_("/tmp/.profile");
	if (!check(dotfile_path != NULL, "dotfile path fixture allocation failed"))
		goto failure;
	dotfile_extension = UArray_pathExtension(dotfile_path);
	dotfile_file = UArray_fileName(dotfile_path);
	if (!check(UArray_findPathExtension(dotfile_path) == -1 &&
		dotfile_extension != NULL && UArray_size(dotfile_extension) == 0,
		"dotfile must not treat its leading dot as an extension") ||
		!check(dotfile_file != NULL &&
			strcmp(UArray_asCString(dotfile_file), ".profile") == 0,
			"dotfile name extraction failed"))
		goto failure;

	trailing_path = UArray_newWithCString_("/tmp/alpha.tar.gz///");
	if (!check(trailing_path != NULL, "trailing-separator path fixture allocation failed"))
		goto failure;
	trailing_component = UArray_lastPathComponent(trailing_path);
	trailing_extension = UArray_pathExtension(trailing_path);
	trailing_file_name = UArray_fileName(trailing_path);
	if (!check(UArray_findLastPathComponent(trailing_path) == 5 &&
		trailing_component != NULL &&
		strcmp(UArray_asCString(trailing_component), "alpha.tar.gz") == 0,
		"trailing separators leaked into the last path component") ||
		!check(trailing_extension != NULL &&
			strcmp(UArray_asCString(trailing_extension), "gz") == 0,
			"trailing separators leaked into extension parsing") ||
		!check(trailing_file_name != NULL &&
			strcmp(UArray_asCString(trailing_file_name), "alpha.tar") == 0,
			"trailing separators leaked into file-name extraction"))
		goto failure;
	UArray_removeLastPathComponent(trailing_path);
	if (!check(strcmp(UArray_asCString(trailing_path), "/tmp") == 0,
		"removing a component after trailing separators did not preserve its parent"))
		goto failure;

	clip_path = UArray_newWithCString_("/tmp/alpha.tar.gz///");
	if (!check(clip_path != NULL, "clip path fixture allocation failed"))
		goto failure;
	UArray_clipBeforeLastPathComponent(clip_path);
	if (!check(strcmp(UArray_asCString(clip_path), "alpha.tar.gz") == 0,
		"clipping before the last component retained trailing separators"))
		goto failure;

	root_path = UArray_newWithCString_("///");
	if (!check(root_path != NULL, "root path fixture allocation failed"))
		goto failure;
	root_component = UArray_lastPathComponent(root_path);
	UArray_removeLastPathComponent(root_path);
	if (!check(UArray_findLastPathComponent(root_path) == 0 &&
		root_component != NULL && UArray_size(root_component) == 0 &&
		UArray_size(root_path) == 3 &&
		memcmp(UArray_asCString(root_path), "///", 3) == 0,
		"root-only path was treated as a removable component"))
		goto failure;

	malformed = UArray_newWithCString_("/tmp/unsafe");
	safe_path = UArray_newWithCString_("/safe");
	if (!check(malformed != NULL && safe_path != NULL,
		"malformed path fixture allocation failed"))
		goto failure;
	malformed_size = malformed->size;

	/* A byte-backed path advertised as uint64 must be rejected before typed scans. */
	malformed->itemType = CTYPE_uint64_t;
	if (!check(UArray_findLastPathComponent(malformed) == -1 &&
		UArray_lastPathComponent(malformed) == NULL &&
		UArray_findPathExtension(malformed) == -1 &&
		UArray_pathExtension(malformed) == NULL &&
		UArray_fileName(malformed) == NULL &&
		UArray_asOSPath(malformed) == NULL &&
		UArray_asUnixPath(malformed) == NULL,
		"UArray path operations accepted malformed item metadata"))
		goto failure;
	UArray_removeLastPathComponent(malformed);
	UArray_clipBeforeLastPathComponent(malformed);
	UArray_removePathExtension(malformed);
	UArray_appendPath_(malformed, safe_path);
	UArray_appendPath_(safe_path, malformed);
	if (!check(malformed->size == malformed_size &&
		memcmp(malformed->data, "/tmp/unsafe", malformed_size) == 0 &&
		UArray_size(safe_path) == 5 &&
		memcmp(safe_path->data, "/safe", 5) == 0,
		"UArray path mutation changed malformed or valid storage"))
		goto failure;

	malformed->itemType = CTYPE_uint8_t;
	malformed->itemSize = 0;
	if (!check(UArray_findLastPathComponent(malformed) == -1 &&
		UArray_pathExtension(malformed) == NULL,
		"UArray path operations accepted zero-width item geometry"))
		goto failure;
	malformed->itemSize = sizeof(uint8_t);

	{
		size_t original_size = malformed->size;
		malformed->size = (size_t)LONG_MAX + (size_t)1;
		{
			int rejected = UArray_findLastPathComponent(malformed) == -1 &&
				UArray_lastPathComponent(malformed) == NULL;
			malformed->size = original_size;
			if (!check(rejected,
				"UArray path operations accepted unrepresentable size"))
				goto failure;
		}
	}

	{
		unsigned char *original_data = malformed->data;
		malformed->data = NULL;
		{
			int rejected = UArray_findLastPathComponent(malformed) == -1 &&
				UArray_pathExtension(malformed) == NULL;
			malformed->data = original_data;
			if (!check(rejected,
				"UArray path operations accepted missing backing storage"))
				goto failure;
		}
	}

	UArray_free(path);
	UArray_free(component);
	UArray_free(extension);
	UArray_free(file_name);
	UArray_free(no_extension);
	UArray_free(empty_extension);
	UArray_free(path_with_directory_dot);
	UArray_free(directory_dot_extension);
	UArray_free(directory_dot_file);
	UArray_free(dotfile_path);
	UArray_free(dotfile_extension);
	UArray_free(dotfile_file);
	UArray_free(trailing_path);
	UArray_free(trailing_component);
	UArray_free(trailing_extension);
	UArray_free(trailing_file_name);
	UArray_free(clip_path);
	UArray_free(root_path);
	UArray_free(root_component);
	UArray_free(malformed);
	UArray_free(safe_path);
	puts("UArray path null-safety test passed");
	return EXIT_SUCCESS;

failure:
	UArray_free(path);
	UArray_free(component);
	UArray_free(extension);
	UArray_free(file_name);
	UArray_free(no_extension);
	UArray_free(empty_extension);
	UArray_free(path_with_directory_dot);
	UArray_free(directory_dot_extension);
	UArray_free(directory_dot_file);
	UArray_free(dotfile_path);
	UArray_free(dotfile_extension);
	UArray_free(dotfile_file);
	UArray_free(trailing_path);
	UArray_free(trailing_component);
	UArray_free(trailing_extension);
	UArray_free(trailing_file_name);
	UArray_free(clip_path);
	UArray_free(root_path);
	UArray_free(root_component);
	UArray_free(malformed);
	UArray_free(safe_path);
	return EXIT_FAILURE;
}
