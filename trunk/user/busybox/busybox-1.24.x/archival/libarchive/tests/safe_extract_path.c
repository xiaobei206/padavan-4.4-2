#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../safe_extract_path.h"

int
main(void)
{
	char temp_root[] = "/tmp/bb-safe-extract-XXXXXX";
	int fd;

	assert(mkdtemp(temp_root) != NULL);
	assert(chdir(temp_root) == 0);
	assert(mkdir("safe", 0700) == 0);

	fd = open("plain", O_WRONLY | O_CREAT | O_EXCL, 0600);
	assert(fd >= 0);
	assert(close(fd) == 0);
	assert(symlink("/etc", "escape") == 0);

	assert(!bb_archive_has_unsafe_parent("safe/file"));
	assert(!bb_archive_has_unsafe_parent("safe/missing/file"));
	assert(!bb_archive_has_unsafe_parent("escape"));
	assert(bb_archive_has_unsafe_parent("escape/passwd"));
	assert(bb_archive_has_unsafe_parent("plain/child"));
	assert(bb_archive_has_unsafe_parent("../etc/passwd"));
	assert(bb_archive_has_unsafe_parent("safe/../etc/passwd"));
	assert(bb_archive_has_unsafe_parent("/etc/passwd"));
	assert(bb_archive_has_unsafe_parent(NULL));
	assert(bb_archive_has_unsafe_parent(""));

	assert(unlink("escape") == 0);
	assert(unlink("plain") == 0);
	assert(rmdir("safe") == 0);
	assert(chdir("/") == 0);
	assert(rmdir(temp_root) == 0);
	return 0;
}
