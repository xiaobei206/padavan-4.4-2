#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../firewall_rule_file.h"

int
main(void)
{
	char directory[] = "/tmp/firewall-rule-file-test.XXXXXX";
	char legacy_path[256], sentinel_path[256], template_path[256];
	char actual_path[256], contents[64];
	char failure_template[] = "/tmp/firewall-rule-file-fail.XXXXXX";
	char failure_path[256];
	FILE *fp, *failure_fp;
	struct stat st;
	int finish_result, finish_errno;

	assert(mkdtemp(directory) != NULL);
	assert(snprintf(legacy_path, sizeof(legacy_path), "%s/ipt_filter.rules", directory) > 0);
	assert(snprintf(sentinel_path, sizeof(sentinel_path), "%s/sentinel", directory) > 0);
	assert(snprintf(template_path, sizeof(template_path), "%s/ipt_filter.XXXXXX", directory) > 0);
	fp = fopen(sentinel_path, "w");
	assert(fp != NULL);
	assert(fputs("preserve-me\n", fp) >= 0);
	assert(fclose(fp) == 0);

	/* A legacy fixed name may already be a symlink to another file. */
	assert(symlink(sentinel_path, legacy_path) == 0);
	fp = firewall_rule_file_open(template_path);
	assert(fp != NULL);
	assert(snprintf(actual_path, sizeof(actual_path), "%s", template_path) > 0);
	assert(strcmp(actual_path, legacy_path) != 0);
	assert(lstat(legacy_path, &st) == 0 && S_ISLNK(st.st_mode));
	assert(stat(actual_path, &st) == 0);
	assert((st.st_mode & 0777) == 0600);
	assert(fputs("*filter\nCOMMIT\n", fp) >= 0);
	assert(firewall_rule_file_finish(fp, template_path) == 0);
	assert(strcmp(template_path, actual_path) == 0);

	fp = fopen(actual_path, "r");
	assert(fp != NULL);
	assert(fgets(contents, sizeof(contents), fp) != NULL);
	assert(strcmp(contents, "*filter\n") == 0);
	assert(fclose(fp) == 0);
	firewall_rule_file_remove(template_path);
	assert(template_path[0] == '\0');
	assert(access(actual_path, F_OK) != 0 && errno == ENOENT);
	assert(lstat(legacy_path, &st) == 0 && S_ISLNK(st.st_mode));

	fp = fopen(sentinel_path, "r");
	assert(fp != NULL);
	assert(fgets(contents, sizeof(contents), fp) != NULL);
	assert(strcmp(contents, "preserve-me\n") == 0);
	assert(fclose(fp) == 0);
	assert(unlink(legacy_path) == 0);
	assert(unlink(sentinel_path) == 0);
	/* A write/close failure must remove the incomplete rules and preserve errno. */
	failure_fp = firewall_rule_file_open(failure_template);
	assert(failure_fp != NULL);
	assert(snprintf(failure_path, sizeof(failure_path), "%s", failure_template) > 0);
	assert(fputs("incomplete rules\n", failure_fp) >= 0);
	assert(close(fileno(failure_fp)) == 0);
	errno = 0;
	finish_result = firewall_rule_file_finish(failure_fp, failure_template);
	finish_errno = errno;
	assert(finish_result == -1);
	assert(finish_errno != 0);
	assert(failure_template[0] == '\0');
	assert(access(failure_path, F_OK) != 0 && errno == ENOENT);

	assert(rmdir(directory) == 0);
	return 0;
}
