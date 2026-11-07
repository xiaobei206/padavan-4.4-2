#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../modem_node_file.h"

int
main(void)
{
	char root[] = "/tmp/modem-node-policy.XXXXXX";
	char directory[PATH_MAX], outside[PATH_MAX], outside_file[PATH_MAX];
	char node_path[PATH_MAX], link_path[PATH_MAX], escape_path[PATH_MAX];
	struct stat st;
	FILE *fp;
	char line[64];
	int n;

	assert(modem_node_name_is_safe("ttyUSB0"));
	assert(modem_node_name_is_safe("ttyACM12"));
	assert(modem_node_name_is_safe("cdc-wdm0"));
	assert(modem_node_name_is_safe("wwan0"));
	assert(modem_node_name_is_safe("weth0"));
	assert(!modem_node_name_is_safe(NULL));
	assert(!modem_node_name_is_safe(""));
	assert(!modem_node_name_is_safe("."));
	assert(!modem_node_name_is_safe(".."));
	assert(!modem_node_name_is_safe("../passwd"));
	assert(!modem_node_name_is_safe("tty/USB0"));
	assert(!modem_node_name_is_safe("tty USB0"));
	assert(!modem_node_name_is_safe("tty\nUSB0"));
	assert(!modem_node_name_is_safe("9ttyUSB0"));

	assert(mkdtemp(root) != NULL);
	n = snprintf(directory, sizeof(directory), "%s/modem", root);
	assert(n > 0 && n < (int)sizeof(directory));
	n = snprintf(outside, sizeof(outside), "%s/outside", root);
	assert(n > 0 && n < (int)sizeof(outside));
	n = snprintf(outside_file, sizeof(outside_file), "%s/sentinel", root);
	assert(n > 0 && n < (int)sizeof(outside_file));
	n = snprintf(node_path, sizeof(node_path), "%s/ttyUSB0", directory);
	assert(n > 0 && n < (int)sizeof(node_path));
	n = snprintf(link_path, sizeof(link_path), "%s/ttyUSB1", directory);
	assert(n > 0 && n < (int)sizeof(link_path));
	n = snprintf(escape_path, sizeof(escape_path), "%s/escape", root);
	assert(n > 0 && n < (int)sizeof(escape_path));

	assert(modem_node_file_write(directory, "ttyUSB0", "pref=1\ndevnum=7\n") == 0);
	assert(lstat(directory, &st) == 0 && S_ISDIR(st.st_mode));
	assert((st.st_mode & 0777) == 0755);
	assert(st.st_uid == geteuid());
	assert(lstat(node_path, &st) == 0 && S_ISREG(st.st_mode));
	assert((st.st_mode & 0777) == 0644);
	assert(st.st_uid == geteuid());
	fp = fopen(node_path, "r");
	assert(fp != NULL);
	assert(fgets(line, sizeof(line), fp) != NULL);
	assert(strcmp(line, "pref=1\n") == 0);
	assert(fclose(fp) == 0);

	fp = fopen(outside_file, "w");
	assert(fp != NULL);
	assert(fputs("sentinel\n", fp) >= 0);
	assert(fclose(fp) == 0);
	assert(symlink(outside_file, link_path) == 0);
	assert(modem_node_file_write(directory, "ttyUSB1", "pref=0\ndevnum=9\n") == 0);
	assert(lstat(link_path, &st) == 0 && S_ISREG(st.st_mode));
	fp = fopen(outside_file, "r");
	assert(fp != NULL);
	assert(fgets(line, sizeof(line), fp) != NULL);
	assert(strcmp(line, "sentinel\n") == 0);
	assert(fclose(fp) == 0);

	assert(mkdir(outside, 0700) == 0);
	assert(symlink(outside, escape_path) == 0);
	assert(modem_node_dir_prepare(escape_path) == -1);
	assert(modem_node_file_write(escape_path, "ttyUSB2", "pref=1\n") == -1);

	assert(modem_node_file_remove(directory, "ttyUSB0") == 0);
	assert(lstat(node_path, &st) != 0);
	assert(modem_node_file_remove(directory, "../sentinel") == -1);

	unlink(link_path);
	unlink(outside_file);
	unlink(escape_path);
	rmdir(outside);
	rmdir(directory);
	rmdir(root);
	puts("USB modem node private metadata staging: PASS");
	return 0;
}
