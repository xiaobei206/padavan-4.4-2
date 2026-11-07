#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dnsmasq_file.h"

static void
read_exact(const char *path, const char *expected)
{
	char buf[128];
	FILE *fp = fopen(path, "r");
	assert(fp != NULL);
	assert(fgets(buf, sizeof(buf), fp) != NULL);
	assert(strcmp(buf, expected) == 0);
	assert(fclose(fp) == 0);
}

int
main(void)
{
	char directory[] = "/tmp/dnsmasq-file-test.XXXXXX";
	char sentinel[256], destination[256], template[256], staged[256];
	char fail_template[256], fail_path[256], lease_path[256];
	char symlink_lease[256], hardlink_lease[256], fifo_lease[256];
	FILE *fp;
	struct stat st;
	int saved_errno;

	assert(mkdtemp(directory) != NULL);
	assert(snprintf(sentinel, sizeof(sentinel), "%s/sentinel", directory) > 0);
	assert(snprintf(destination, sizeof(destination), "%s/dnsmasq.servers", directory) > 0);
	assert(snprintf(template, sizeof(template), "%s/dnsmasq.servers.XXXXXX", directory) > 0);
	assert(snprintf(fail_template, sizeof(fail_template), "%s/failed.XXXXXX", directory) > 0);
	assert(snprintf(lease_path, sizeof(lease_path), "%s/dnsmasq.leases", directory) > 0);
	assert(snprintf(symlink_lease, sizeof(symlink_lease), "%s/symlink.leases", directory) > 0);
	assert(snprintf(hardlink_lease, sizeof(hardlink_lease), "%s/hardlink.leases", directory) > 0);
	assert(snprintf(fifo_lease, sizeof(fifo_lease), "%s/fifo.leases", directory) > 0);

	fp = fopen(sentinel, "w");
	assert(fp != NULL);
	assert(fputs("preserve-me\n", fp) >= 0);
	assert(fclose(fp) == 0);
	assert(chmod(sentinel, 0644) == 0);

	/* Publishing over a planted symlink replaces the link, not its target. */
	assert(symlink(sentinel, destination) == 0);
	assert(dnsmasq_file_is_safe_regular(destination) == -1);
	fp = dnsmasq_file_open(template);
	assert(fp != NULL);
	assert(snprintf(staged, sizeof(staged), "%s", template) > 0);
	assert(stat(staged, &st) == 0);
	assert((st.st_mode & 0777) == 0600);
	assert(fputs("server=/example.test/192.0.2.53\n", fp) >= 0);
	assert(dnsmasq_file_publish(fp, template, destination, 0644) == 0);
	assert(template[0] == '\0');
	assert(lstat(destination, &st) == 0 && S_ISREG(st.st_mode));
	assert(st.st_uid == geteuid() && st.st_nlink == 1);
	assert((st.st_mode & 0777) == 0644);
	assert(dnsmasq_file_is_safe_regular(destination) == 0);
	assert(chmod(destination, 0666) == 0);
	assert(dnsmasq_file_is_safe_regular(destination) == -1);
	assert(chmod(destination, 0644) == 0);
	read_exact(destination, "server=/example.test/192.0.2.53\n");
	read_exact(sentinel, "preserve-me\n");

	/* Finalization errors retain errno, unlink staging and preserve old output. */
	fp = dnsmasq_file_open(fail_template);
	assert(fp != NULL);
	assert(snprintf(fail_path, sizeof(fail_path), "%s", fail_template) > 0);
	assert(fputs("partial\n", fp) >= 0);
	assert(close(fileno(fp)) == 0);
	errno = 0;
	assert(dnsmasq_file_publish(fp, fail_template, destination, 0644) == -1);
	saved_errno = errno;
	assert(saved_errno != 0);
	assert(fail_template[0] == '\0');
	assert(access(fail_path, F_OK) != 0 && errno == ENOENT);
	read_exact(destination, "server=/example.test/192.0.2.53\n");

	/* Lease file initialization must reject a symlink without touching target. */
	assert(symlink(sentinel, symlink_lease) == 0);
	assert(dnsmasq_lease_file_ensure(symlink_lease) == -1);
	assert(lstat(symlink_lease, &st) == 0 && S_ISLNK(st.st_mode));
	read_exact(sentinel, "preserve-me\n");

	/* Existing regular lease files are preserved and normalized to safe mode. */
	fp = fopen(lease_path, "w");
	assert(fp != NULL);
	assert(fputs("lease-record\n", fp) >= 0);
	assert(fclose(fp) == 0);
	assert(chmod(lease_path, 0666) == 0);
	assert(dnsmasq_lease_file_ensure(lease_path) == 0);
	assert(stat(lease_path, &st) == 0 && S_ISREG(st.st_mode));
	assert(st.st_uid == geteuid() && st.st_nlink == 1);
	assert((st.st_mode & 0777) == 0644);
	read_exact(lease_path, "lease-record\n");

	/* Do not chmod a hard-linked victim or block while opening a FIFO. */
	assert(link(sentinel, hardlink_lease) == 0);
	assert(dnsmasq_file_is_safe_regular(hardlink_lease) == -1);
	assert(dnsmasq_lease_file_ensure(hardlink_lease) == -1);
	assert(stat(sentinel, &st) == 0 && (st.st_mode & 0777) == 0644);
	assert(mkfifo(fifo_lease, 0600) == 0);
	assert(dnsmasq_file_is_safe_regular(fifo_lease) == -1);
	assert(dnsmasq_lease_file_ensure(fifo_lease) == -1);
	assert(lstat(fifo_lease, &st) == 0 && S_ISFIFO(st.st_mode));
	read_exact(sentinel, "preserve-me\n");

	assert(unlink(hardlink_lease) == 0);
	assert(unlink(fifo_lease) == 0);
	assert(unlink(symlink_lease) == 0);
	assert(unlink(lease_path) == 0);
	assert(unlink(destination) == 0);
	assert(unlink(sentinel) == 0);
	assert(rmdir(directory) == 0);
	return 0;
}
