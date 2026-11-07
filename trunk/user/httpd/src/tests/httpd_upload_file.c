#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "httpd_upload_file.h"

static void
write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "w");
    assert(stream != NULL);
    assert(fputs(text, stream) >= 0);
    assert(fclose(stream) == 0);
}

static void
assert_text(const char *path, const char *expected)
{
    char buffer[128];
    FILE *stream = fopen(path, "r");
    assert(stream != NULL);
    assert(fgets(buffer, sizeof(buffer), stream) != NULL);
    assert(strcmp(buffer, expected) == 0);
    assert(fclose(stream) == 0);
}

int
main(void)
{
    char dir[] = "/tmp/httpd-upload-exclusive.XXXXXX";
    char target[256], victim[256];
    FILE *stream;
    struct stat info;
    int n;

    assert(mkdtemp(dir) != NULL);
    n = snprintf(target, sizeof(target), "%s/upload.bin", dir);
    assert(n > 0 && (size_t)n < sizeof(target));
    n = snprintf(victim, sizeof(victim), "%s/victim.txt", dir);
    assert(n > 0 && (size_t)n < sizeof(victim));

    write_text(victim, "do not overwrite\n");
    assert(symlink(victim, target) == 0);

    errno = 0;
    stream = httpd_upload_open_exclusive(target);
    assert(stream == NULL);
    assert(errno == EEXIST || errno == ELOOP);
    assert_text(victim, "do not overwrite\n");
    assert(unlink(target) == 0);

    stream = httpd_upload_open_exclusive(target);
    assert(stream != NULL);
    assert(fputs("uploaded safely\n", stream) >= 0);
    assert(fclose(stream) == 0);
    assert(lstat(target, &info) == 0);
    assert(S_ISREG(info.st_mode));
    assert((info.st_mode & 0777) == 0600);
    assert_text(target, "uploaded safely\n");
    assert_text(victim, "do not overwrite\n");

    assert(unlink(target) == 0);
    assert(unlink(victim) == 0);
    assert(rmdir(dir) == 0);
    puts("HTTPD upload exclusive-create regression: OK");
    return 0;
}
