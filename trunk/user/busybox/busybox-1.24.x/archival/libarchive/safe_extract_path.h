#ifndef BB_ARCHIVE_SAFE_EXTRACT_PATH_H
#define BB_ARCHIVE_SAFE_EXTRACT_PATH_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Archive member names are relative to the extraction directory. Reject an
 * absolute path, a parent component, or a path whose existing ancestor is not
 * a directory. In particular, lstat() detects symlink ancestors before the
 * extractor can unlink/open a child through them.
 *
 * Missing ancestors are allowed because tar can create them as archive
 * members arrive. Each later member is checked again before filesystem writes.
 */
static int
bb_archive_has_unsafe_parent(const char *path)
{
	char *copy;
	char *component;
	char *cursor;
	struct stat st;

	if (!path || !path[0] || path[0] == '/')
		return 1;

	copy = malloc(strlen(path) + 1);
	if (!copy)
		return 1;
	strcpy(copy, path);

	component = copy;
	for (cursor = copy; ; ++cursor) {
		char delimiter;

		if (*cursor != '/' && *cursor != '\0')
			continue;

		delimiter = *cursor;
		*cursor = '\0';

		if (strcmp(component, "..") == 0) {
			free(copy);
			return 1;
		}

		/* Check ancestors only; the entry itself may legitimately be a link. */
		if (delimiter == '/' && component[0] != '\0') {
			if (lstat(copy, &st) == 0) {
				if (!S_ISDIR(st.st_mode)) {
					free(copy);
					return 1;
				}
			} else if (errno != ENOENT) {
				free(copy);
				return 1;
			}
		}

		if (delimiter == '\0')
			break;

		*cursor = delimiter;
		component = cursor + 1;
	}

	free(copy);
	return 0;
}

#endif /* BB_ARCHIVE_SAFE_EXTRACT_PATH_H */
