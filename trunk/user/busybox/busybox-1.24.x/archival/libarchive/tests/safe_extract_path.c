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

	assert(!bb_archive_path_has_unsafe_component("safe/file", 0));
	assert(!bb_archive_path_has_unsafe_component("safe/missing/file", 0));
	assert(!bb_archive_path_has_unsafe_component("escape", 0));
	assert(bb_archive_path_has_unsafe_component("escape/passwd", 0));
	assert(bb_archive_path_has_unsafe_component("escape", 1));
	assert(bb_archive_path_has_unsafe_component("plain", 1));
	assert(!bb_archive_path_has_unsafe_component("safe", 1));
	assert(!bb_archive_path_has_unsafe_component("safe/missing", 1));
	assert(bb_archive_path_has_unsafe_component("plain/child", 0));
	assert(bb_archive_path_has_unsafe_component("../etc/passwd", 0));
	assert(bb_archive_path_has_unsafe_component("safe/../etc/passwd", 0));
	assert(bb_archive_path_has_unsafe_component("/etc/passwd", 0));
	assert(bb_archive_path_has_unsafe_component(NULL, 0));
	assert(bb_archive_path_has_unsafe_component("", 0));

	assert(unlink("escape") == 0);
	assert(unlink("plain") == 0);
	assert(rmdir("safe") == 0);
	assert(chdir("/") == 0);
	assert(rmdir(temp_root) == 0);
	return 0;
}
