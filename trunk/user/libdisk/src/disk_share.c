/*
 *  * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <nvram_linux.h>
#include <shutils.h>

#include "disk_io_tools.h"
#include "disk_initial.h"
#include "disk_share.h"

static void nvram_commit_safe()
{
	if (nvram_get_int("nvram_manual") == 1)
		return;

	nvram_commit();
}

static int
is_valid_account_name(const char *name)
{
	const unsigned char *p;
	size_t len;

	if (!name || !*name)
		return 0;

	len = strlen(name);
	if (len > 31 || !strcmp(name, ".") || !strcmp(name, ".."))
		return 0;

	for (p = (const unsigned char *)name; *p; ++p) {
		if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
			return 0;
	}

	return 1;
}

static int
is_valid_folder_name(const char *name)
{
	const unsigned char *p;
	size_t len;

	if (!name || !*name)
		return 0;

	len = strlen(name);
	if (len > 255 || !strcmp(name, ".") || !strcmp(name, ".."))
		return 0;

	for (p = (const unsigned char *)name; *p; ++p) {
		if (*p < 32 || *p == 127 || *p == '/' || *p == '\\' ||
		    *p == '[' || *p == ']' || *p == '\r' || *p == '\n')
			return 0;
	}

	return 1;
}

int get_account_list(int *acc_num, char ***account_list)
{
	char nvram_name[24], *nvram_value, **tmp_account_list;
	int acc_max, len, i, j, count, seen_empty;

	if (!acc_num || !account_list)
		return -1;

	*acc_num = 0;
	*account_list = NULL;

	acc_max = nvram_get_int("acc_num");
	if (acc_max > MAX_ACCOUNT_NUM)
		acc_max = MAX_ACCOUNT_NUM;
	if (acc_max <= 0)
		return 0;

	tmp_account_list = (char **)calloc((size_t)acc_max, sizeof(char *));
	if (!tmp_account_list)
		return -1;

	count = 0;
	seen_empty = 0;
	for (i = 0; i < acc_max; ++i) {
		snprintf(nvram_name, sizeof(nvram_name), "acc_username%d", i);
		nvram_value = nvram_safe_get(nvram_name);
		len = strlen(nvram_value);
		if (len < 1) {
			seen_empty = 1;
			continue;
		}
		/* Account management uses the count as the next NVRAM index. */
		if (!is_valid_account_name(nvram_value) ||
		    !strcmp(nvram_value, FTP_ANONYMOUS_USER) || seen_empty) {
			for (j = 0; j < count; ++j)
				free(tmp_account_list[j]);
			free(tmp_account_list);
			return -1;
		}

		tmp_account_list[count] = (char *)malloc((size_t)len + 1);
		if (!tmp_account_list[count]) {
			for (j = 0; j < count; ++j)
				free(tmp_account_list[j]);
			free(tmp_account_list);
			return -1;
		}

		memcpy(tmp_account_list[count], nvram_value, (size_t)len + 1);
		++count;
	}

	if (count == 0) {
		free(tmp_account_list);
		return 0;
	}

	*acc_num = count;
	*account_list = tmp_account_list;
	return 0;
}
int get_folder_list_in_mount_path(const char *const mount_path, int *sh_num, char ***folder_list)
{
	char **tmp_folder_list = NULL;
	char target[32];
	char *list_file = NULL, *list_info = NULL;
	char *follow_info, *line_end, *endptr;
	int i, count;
	long parsed_count;
	size_t list_file_len, list_info_len, name_len;
	int target_len;

	if (!mount_path || !*mount_path || !sh_num || !folder_list)
		return -1;

	*sh_num = 0;
	*folder_list = NULL;

	list_file_len = strlen(mount_path) + strlen("/.__folder_list.txt");
	if (list_file_len < strlen(mount_path))
		return -1;
	list_file = (char *)malloc(list_file_len + 1);
	if (!list_file)
		return -1;

	snprintf(list_file, list_file_len + 1, "%s/.__folder_list.txt", mount_path);

	if (!test_if_file(list_file))
		initial_folder_list_in_mount_path(mount_path);

	list_info = read_whole_file(list_file);
	free(list_file);
	if (!list_info)
		return -1;

	list_info_len = strlen(list_info);
	follow_info = strstr(list_info, "sh_num=");
	if (!follow_info)
		goto invalid_list;

	follow_info += strlen("sh_num=");
	if (*follow_info < '0' || *follow_info > '9')
		goto invalid_list;

	parsed_count = strtol(follow_info, &endptr, 10);
	if (endptr == follow_info || parsed_count < 0 || parsed_count > INT_MAX)
		goto invalid_list;
	if (*endptr != '\n' && !(*endptr == '\r' && endptr[1] == '\n'))
		goto invalid_list;

	count = (int)parsed_count;
	if (count == 0) {
		free(list_info);
		return 0;
	}

	/* Each entry needs at least: newline + "sh_nameN=" + newline. */
	if ((size_t)count > list_info_len / 11)
		goto invalid_list;

	tmp_folder_list = (char **)calloc((size_t)count, sizeof(char *));
	if (!tmp_folder_list)
		goto invalid_list;

	for (i = 0; i < count; ++i) {
		target_len = snprintf(target, sizeof(target), "\nsh_name%d=", i);
		if (target_len < 0 || target_len >= (int)sizeof(target))
			goto invalid_entries;

		follow_info = strstr(list_info, target);
		if (!follow_info)
			goto invalid_entries;

		follow_info += target_len;
		line_end = strchr(follow_info, '\n');
		if (!line_end)
			goto invalid_entries;

		name_len = (size_t)(line_end - follow_info);
		if (name_len == 0)
			goto invalid_entries;

		tmp_folder_list[i] = (char *)malloc(name_len + 1);
		if (!tmp_folder_list[i])
			goto invalid_entries;

		memcpy(tmp_folder_list[i], follow_info, name_len);
		tmp_folder_list[i][name_len] = '\0';
		if (!is_valid_folder_name(tmp_folder_list[i]))
			goto invalid_entries;
	}

	free(list_info);
	*sh_num = count;
	*folder_list = tmp_folder_list;
	return 0;

invalid_entries:
	free_2_dimension_list(&count, &tmp_folder_list);
invalid_list:
	free(list_info);
	return -1;
}
int get_all_folder_in_mount_path(const char *const mount_path, int *sh_num, char ***folder_list)
{
	DIR *pool_to_open;
	struct dirent *dp;
	char *testdir, *folder_name;
	char **tmp_folder_list = NULL, **new_folder_list;
	int count = 0, i;
	size_t mount_len, name_len, path_len;

	if (!mount_path || !*mount_path || !sh_num || !folder_list)
		return -1;

	*sh_num = 0;
	*folder_list = NULL;
	mount_len = strlen(mount_path);

	pool_to_open = opendir(mount_path);
	if (!pool_to_open)
		return -1;

	while ((dp = readdir(pool_to_open)) != NULL) {
		if (dp->d_name[0] == '.')
			continue;
		if (test_if_System_folder(dp->d_name) == 1)
			continue;

		name_len = strlen(dp->d_name);
		if (!is_valid_folder_name(dp->d_name))
			continue;
		if (mount_len > (size_t)-1 - name_len - 2)
			goto failed;

		path_len = mount_len + name_len + 1;
		testdir = (char *)malloc(path_len + 1);
		if (!testdir)
			goto failed;

		snprintf(testdir, path_len + 1, "%s/%s", mount_path, dp->d_name);
		if (!test_if_dir(testdir)) {
			free(testdir);
			continue;
		}
		free(testdir);

		if (count == INT_MAX ||
		    (size_t)count >= (size_t)-1 / sizeof(*tmp_folder_list))
			goto failed;

		folder_name = (char *)malloc(name_len + 1);
		if (!folder_name)
			goto failed;
		memcpy(folder_name, dp->d_name, name_len + 1);

		new_folder_list = (char **)realloc(tmp_folder_list,
						  sizeof(*tmp_folder_list) * (size_t)(count + 1));
		if (!new_folder_list) {
			free(folder_name);
			goto failed;
		}

		tmp_folder_list = new_folder_list;
		tmp_folder_list[count++] = folder_name;
	}

	closedir(pool_to_open);
	*sh_num = count;
	*folder_list = tmp_folder_list;
	return 0;

failed:
	closedir(pool_to_open);
	for (i = 0; i < count; ++i)
		free(tmp_folder_list[i]);
	free(tmp_folder_list);
	return -1;
}
void free_2_dimension_list(int *num, char ***list)
{
	int i;
	char **target;

	if (!num || !list)
		return;

	target = *list;
	if (target) {
		for (i = 0; i < *num; ++i)
			if (target[i] != NULL)
				free(target[i]);
		free(target);
	}

	*num = 0;
	*list = NULL;
}

int initial_folder_list_in_mount_path(const char *const mount_path)
{
	int sh_num = 0;
	char **folder_list = NULL;
	FILE *fp;
	char *list_file;
	int result, len, i;

	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;

	// 2. get the folder number and folder_list
	result = get_all_folder_in_mount_path(mount_path, &sh_num, &folder_list);
	if (result != 0) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}

	// 3. get the list_file
	len = strlen(mount_path)+strlen("/.__folder_list.txt");
	list_file = (char *)malloc(sizeof(char)*(len+1));
	if (list_file == NULL) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}
	sprintf(list_file, "%s/.__folder_list.txt", mount_path);
	list_file[len] = 0;

	// 4. write the folder info
	fp = fopen(list_file, "w");
	if (fp == NULL) {
		free_2_dimension_list(&sh_num, &folder_list);
		free(list_file);
		
		return -1;
	}
	free(list_file);

	fprintf(fp, "sh_num=%d\n", sh_num);
	for (i = 0; i < sh_num; ++i)
		fprintf(fp, "sh_name%d=%s\n", i, folder_list[i]);
	fclose(fp);

	free_2_dimension_list(&sh_num, &folder_list);
	return 0;
}

int initial_one_var_file_in_mount_path(const char *const account, const char *const mount_path)
{
	FILE *fp;
	char var_file[256];
	int result, i;
	int sh_num = 0;
	char **folder_list = NULL;
	int samba_right, ftp_right;

	if (!is_valid_account_name(account))
		return -1;
	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;

	// get the folder number and folder_list
	result = get_all_folder_in_mount_path(mount_path, &sh_num, &folder_list);
	if (result != 0) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}

	// get the samba right and ftp right
	if (strcmp(account, FTP_ANONYMOUS_USER) == 0/* || strcmp(account, SMB_GUEST_USER) == 0*/) {
		samba_right = 0;
		ftp_right = 1;
	}
	else {
		samba_right = DEFAULT_SAMBA_RIGHT;
		ftp_right = DEFAULT_FTP_RIGHT;
	}

	// write the default content in the var file
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account);
	if ((fp = fopen(var_file, "w")) == NULL) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}
	for (i = 0; i < sh_num; ++i) {
		fprintf(fp, "*");
		
		fprintf(fp, "%s", folder_list[i]);
		fprintf(fp, "=%d%d%d\n", samba_right, ftp_right, DEFAULT_DMS_RIGHT);
	}

	fclose(fp);
	free_2_dimension_list(&sh_num, &folder_list);

	return 0;
}

static void
clear_share_var_files(const char *mount_path)
{
	DIR *dir;
	struct dirent *entry;
	char path[512], account[32 + 1];
	int path_len, managed;
	size_t name_len, account_len;

	dir = opendir(mount_path);
	if (!dir)
		return;

	while ((entry = readdir(dir)) != NULL) {
		name_len = strlen(entry->d_name);
		managed = 0;

		if (strcmp(entry->d_name, ".__folder_list.txt") == 0) {
			managed = 1;
		} else if (name_len > 3 + 8 &&
			   strncmp(entry->d_name, ".__", 3) == 0 &&
			   strcmp(entry->d_name + name_len - 8, "_var.txt") == 0) {
			account_len = name_len - 3 - 8;
			if (account_len > 0 && account_len <= 31) {
				memcpy(account, entry->d_name + 3, account_len);
				account[account_len] = '\0';
				managed = is_valid_account_name(account);
			}
		}

		/* Only remove metadata files owned by this subsystem. */
		if (!managed)
			continue;

		path_len = snprintf(path, sizeof(path), "%s/%s", mount_path, entry->d_name);
		if (path_len < 0 || path_len >= (int)sizeof(path))
			continue;

		unlink(path);
	}

	closedir(dir);
}

int initial_all_var_file_in_mount_path(const char *const mount_path)
{
	int i;
	int acc_num = 0;
	char **account_list = NULL;

	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;

	/* Delete managed files without interpolating the mount path into a shell. */
	clear_share_var_files(mount_path);

	// initial the var file for ftp anonymous
	initial_one_var_file_in_mount_path(FTP_ANONYMOUS_USER, mount_path);

	// get the account number and account_list
	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;

	// initial the var file
	for (i = 0; i < acc_num; ++i)
		initial_one_var_file_in_mount_path(account_list[i], mount_path);
	free_2_dimension_list(&acc_num, &account_list);

	// initial the folder list
	initial_folder_list_in_mount_path(mount_path);

	return 0;
}

int create_if_no_var_files(const char *const mount_path)
{
	int acc_num = 0;
	char **account_list = NULL;
	int i;
	char var_file[256];

	// process aninymous user
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, FTP_ANONYMOUS_USER);
	if (!test_if_file(var_file))
		initial_one_var_file_in_mount_path(FTP_ANONYMOUS_USER, mount_path);
	else
		modify_if_exist_new_folder(FTP_ANONYMOUS_USER, mount_path);

	// get the account number and account_list
	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;

	for (i = 0; i < acc_num; ++i) {
		snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account_list[i]);
		if (!test_if_file(var_file))
			initial_one_var_file_in_mount_path(account_list[i], mount_path);
		else
			modify_if_exist_new_folder(account_list[i], mount_path);
	}

	free_2_dimension_list(&acc_num, &account_list);

	return 0;
}

int modify_if_exist_new_folder(const char *const account, const char *const mount_path)
{
	int sh_num = 0;
	char **folder_list = NULL, *target;
	int result, i, len;
	char var_file[256];
	FILE *fp;
	int samba_right, ftp_right;

	if (!is_valid_account_name(account) || !mount_path || !*mount_path)
		return -1;

	// 1. get all folder in mount_path
	result = get_all_folder_in_mount_path(mount_path, &sh_num, &folder_list);
	if (result != 0) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}

	for (i = 0; i < sh_num; ++i) {
		result = test_if_exist_folder_in_mount_path(mount_path, folder_list[i]);
		if (result != 0) {
			continue;
		}
		
		// get the target
		len = strlen("*")+strlen(folder_list[i])+strlen("=");
		target = (char *)malloc(sizeof(char)*(len+1));
		if (target == NULL) {
			free_2_dimension_list(&sh_num, &folder_list);
			
			return -1;
		}
		sprintf(target, "*%s=", folder_list[i]);
		target[len] = 0;
		
		// get the samba right and ftp right
		if (strcmp(account, FTP_ANONYMOUS_USER) == 0) {
			samba_right = 1;
			ftp_right = 1;
		}
		else {
			samba_right = DEFAULT_SAMBA_RIGHT;
			ftp_right = DEFAULT_FTP_RIGHT;
		}
		
		// add the information of the new folder
		snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account);
		fp = fopen(var_file, "a+");
		if (fp == NULL) {
			free_2_dimension_list(&sh_num, &folder_list);
			free(target);
			return -1;
		}
		
		fprintf(fp, "%s%d%d%d\n", target, samba_right, ftp_right, DEFAULT_DMS_RIGHT);
		free(target);
		fclose(fp);
	}
	free_2_dimension_list(&sh_num, &folder_list);

	return 0;
}

int get_permission(const char *account,
		   const char *mount_path,
		   const char *folder,
		   const char *protocol)
{
	char var_file[256];
	char *target, *follow_info, *var_info;
	int len, result;

	if (!mount_path || !*mount_path || !is_valid_account_name(account) ||
	    !is_valid_folder_name(folder) || !protocol)
		return -1;

	// 2. get the content of the var_file of the account
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account);
	var_info = read_whole_file(var_file);
	if (!var_info)
		return 0;

	// 3. get the target in the content
	len = strlen(folder)+strlen("*=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (!target) {
		free(var_info);
		return -1;
	}
	sprintf(target, "*%s=", folder);
	target[len] = 0;

	follow_info = upper_strstr(var_info, target);
	free(target);
	if (follow_info == NULL) {
		free(var_info);
		return -1;
	}

	follow_info += len;
	if (strlen(follow_info) < 4 || follow_info[3] != '\n') {
		free(var_info);
		return -1;
	}

	// 4. get the right of folder
	if (!strcmp(protocol, "cifs"))
		result = follow_info[0]-'0';
	else if (!strcmp(protocol, "ftp"))
		result = follow_info[1]-'0';
	else if (!strcmp(protocol, "dms"))
		result = follow_info[2]-'0';
	else{
		free(var_info);
		return -1;
	}
	free(var_info);

	if (result < 0 || result > 3)
		return -1;

	return result;
}

int set_permission(const char *account,
		   const char *mount_path,
		   const char *folder,
		   const char *protocol,
		   const int flag)
{
	FILE *fp;
	char var_file[256];
	char *target, *follow_info, *var_info;
	int len, result;

	if (flag < 0 || flag > 3)
		return -1;
	if (!mount_path || !*mount_path || !is_valid_account_name(account) ||
	    !is_valid_folder_name(folder) || !protocol)
		return -1;

	// 2. get the content of the var_file of the account
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account);
	var_info = read_whole_file(var_file);
	if (!var_info) {
		initial_one_var_file_in_mount_path(account, mount_path);
		sleep(1);
		var_info = read_whole_file(var_file);
		if (!var_info)
			return -1;
	}

	// 3. get the target in the content
	len = strlen("*")+strlen(folder)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (!target) {
		free(var_info);
		return -1;
	}
	sprintf(target, "*%s=", folder);
	target[len] = 0;

	// 4. judge if the target is in the var file.
	follow_info = upper_strstr(var_info, target);
	if (!follow_info) {
		free(var_info);
		
		result = initial_folder_list_in_mount_path(mount_path);
		
		fp = fopen(var_file, "a+");
		if (!fp) {
			free(target);
			return -1;
		}
		
		fprintf(fp, "%s", target);
		free(target);
		
		// 5.1 change the right of folder
		if (!strcmp(protocol, "cifs"))
			fprintf(fp, "%d%d%d\n", flag, 0, DEFAULT_DMS_RIGHT);
		else if (!strcmp(protocol, "ftp"))
			fprintf(fp, "%d%d%d\n", 0, flag, DEFAULT_DMS_RIGHT);
		else if (!strcmp(protocol, "dms"))
			fprintf(fp, "%d%d%d\n", 0, 0, flag);
		else{
			fclose(fp);
			return -1;
		}
		fclose(fp);
		
		if (!strcmp(protocol, "cifs"))
			system("/sbin/run_samba");
		
		return 0;
	}
	free(target);

	follow_info += len;
	if (strlen(follow_info) < 4 || follow_info[3] != '\n') {
		free(var_info);
		return -1;
	}

	// 5.2. change the right of folder
	if (!strcmp(protocol, "cifs")) {
		if (follow_info[0] == '0'+flag) {
			free(var_info);
			return 0;
		}
		
		follow_info[0] = '0'+flag;
	}
	else if (!strcmp(protocol, "ftp")) {
		if (follow_info[1] == '0'+flag) {
			free(var_info);
			return 0;
		}
		
		follow_info[1] = '0'+flag;
	}
	else if (!strcmp(protocol, "dms")) {
		if (follow_info[2] == '0'+flag) {
			free(var_info);
			return 0;
		}
		
		follow_info[2] = '0'+flag;
	}
	else{
		free(var_info);
		return -1;
	}

	// 6. rewrite the var file.
	fp = fopen(var_file, "w");
	if (fp == NULL) {
		free(var_info);
		return -1;
	}
	fprintf(fp, "%s", var_info);
	fclose(fp);

	free(var_info);

	if (!strcmp(protocol, "cifs"))
		system("/sbin/run_samba");

	return 0;
}

int add_account(const char *const account, const char *const password)
{
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int i, acc_num = 0;
	char **account_list = NULL;
	char nvram_name[32];

	if (!is_valid_account_name(account) || !strcmp(account, FTP_ANONYMOUS_USER))
		return -1;
	if (password == NULL || strlen(password) <= 0)
		return -1;

	// 1. check if can create the account
	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;
	if (acc_num >= MAX_ACCOUNT_NUM) {
		free_2_dimension_list(&acc_num, &account_list);
		return -1;
	}

	for (i = 0; i < acc_num; ++i)
		if (!strcmp(account, account_list[i])) {
			free_2_dimension_list(&acc_num, &account_list);
			return -1;
		}

	if (!strcmp(account, FTP_ANONYMOUS_USER)) {
		free_2_dimension_list(&acc_num, &account_list);
		return -1;
	}

	// 2. create nvram value about the new account
	nvram_set_int("acc_num", acc_num+1);

	sprintf(nvram_name, "acc_username%d", acc_num);
	nvram_set(nvram_name, account);

	sprintf(nvram_name, "acc_password%d", acc_num);
	nvram_set(nvram_name, password);

	nvram_commit_safe();

	free_2_dimension_list(&acc_num, &account_list);

	// 3. find every pool
	disk_list = read_disk_data();
	if (!disk_list)
		return 0;

	for (follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next) {			
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL)
				continue;
			
			// 4. initial the var file of the account
			initial_one_var_file_in_mount_path(account, follow_partition->mount_point);
		}
	}
	free_disk_data(disk_list);

	// 6. re-run samba
	system("/sbin/run_samba");

	return 0;
}

int del_account(const char *const account)
{
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num = 0, target;
	char **account_list = NULL;
	int result, i;
	char nvram_name[16], nvram_value[128];
	char var_file[256];

	if (!is_valid_account_name(account) || !strcmp(account, FTP_ANONYMOUS_USER))
		return -1;

	// 1. check if can create the account
	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;
	if (acc_num <= 0)
		return -1;

	result = 0;
	for (i = 0; i < acc_num; ++i)
		if (!strcmp(account_list[i], account)) {
			result = 1;
			target = i;
			break;
		}
	if (result == 0) {
		free_2_dimension_list(&acc_num, &account_list);
		return -1;
	}

	// 2. delete the nvram value about the deleted account
	--acc_num;
	nvram_set_int("acc_num", acc_num);

	for (i = target; i < acc_num; ++i) {
		sprintf(nvram_name, "acc_username%d", i);
		nvram_set(nvram_name, account_list[i+1]);
		
		sprintf(nvram_value, "acc_password%d", i+1);
		
		sprintf(nvram_name, "acc_password%d", i);
		nvram_set(nvram_name, nvram_safe_get(nvram_value));
	}

	// 3. change to the share mode when no account
	if (acc_num < 1) {
		int i_mode;
		
		i_mode = nvram_get_int("st_samba_mode");
		if (i_mode != 1 && i_mode != 3)
			nvram_set_int("st_samba_mode", 1);
		
		i_mode = nvram_get_int("st_ftp_mode");
		if (i_mode != 1 && i_mode != 3)
			nvram_set_int("st_ftp_mode", 1);
	}

	nvram_commit_safe();

	free_2_dimension_list(&acc_num, &account_list);

	// 4. find every pool
	disk_list = read_disk_data();
	if (!disk_list)
		return 0;

	for (follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL)
				continue;
			
			// 5. delete the var file of the deleted account
			snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", follow_partition->mount_point, account);
			
			unlink(var_file);
		}
	}
	free_disk_data(disk_list);

	// 6. re-run ftp and samba
	system("/sbin/run_ftpsamba");

	return 0;
}

// "new_account" can be the same with "account"!
int mod_account(const char *const account, const char *const new_account, const char *const new_password)
{
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int i, acc_num = 0;
	char **account_list = NULL;
	char nvram_name[16];
	int account_order = -1;
	char var_file[256], new_var_file[256];

	if (!is_valid_account_name(account) || !strcmp(account, FTP_ANONYMOUS_USER))
		return -1;
	if (new_account && *new_account &&
	    (!is_valid_account_name(new_account) || !strcmp(new_account, FTP_ANONYMOUS_USER)))
		return -1;
	if ((new_account == NULL || strlen(new_account) <= 0) && (new_password == NULL || strlen(new_password) <= 0))
		return -1;

	// 1. check if can modify the account
	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;

	for (i = 0; i < acc_num; ++i) {
		if (new_account != NULL && strcmp(new_account, account) && strlen(new_account) > 0) {
			if (!strcmp(account_list[i], new_account)) {
				free_2_dimension_list(&acc_num, &account_list);
				
				return -1;
			}
		}
		
		if (!strcmp(account, account_list[i]))
			account_order = i;
	}
	free_2_dimension_list(&acc_num, &account_list);

	if (account_order == -1)
		return -1;

	if (new_account && *new_account && !strcmp(new_account, FTP_ANONYMOUS_USER))
		return -1;

	// 2. modify nvram value about the new account
	if (new_account != NULL && strcmp(new_account, account) && strlen(new_account) > 0) {
		sprintf(nvram_name, "acc_username%d", account_order);
		nvram_set(nvram_name, new_account);
	}

	if (new_password != NULL && strlen(new_password) > 0) {
		sprintf(nvram_name, "acc_password%d", account_order);
		nvram_set(nvram_name, new_password);
	}

	nvram_commit_safe();

	// 3. find every pool
	if (new_account == NULL || strlen(new_account) <= 0/* ||
			(new_account != NULL && !strcmp(new_account, account))*/)
		return 0;

	if ((new_account != NULL && !strcmp(new_account, account)))
		goto rerun;

	disk_list = read_disk_data();
	if (!disk_list)
		return 0;

	for (follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL)
				continue;
			
			// get the var_file and new_var_file
			snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", follow_partition->mount_point, account);
			snprintf(new_var_file, sizeof(new_var_file), "%s/.__%s_var.txt", follow_partition->mount_point, new_account);
			
			// rename the var file
			rename(var_file, new_var_file);
		}
	}
	free_disk_data(disk_list);

rerun:
	// 6. re-run ftp and samba
	system("/sbin/run_ftpsamba");

	return 0;
}

int add_folder(const char *const mount_path, const char *const folder)
{
	int result, i, len;
	int acc_num = 0;
	char **account_list = NULL;
	char var_file[256];
	char *target, *var_info;
	FILE *fp;
	int samba_right, ftp_right;
	char *full_path;

	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;
	if (!is_valid_folder_name(folder))
		return -1;

	// test if creatting the folder
	result = test_if_exist_folder_in_mount_path(mount_path, folder);
	if (result != 0)
		return -1;

	// create the folder
	len = strlen(mount_path)+strlen("/")+strlen(folder);
	full_path = (char *)malloc(sizeof(char)*(len+1));
	if (!full_path)
		return -1;
	sprintf(full_path, "%s/%s", mount_path, folder);
	full_path[len] = 0;

	{
		mode_t old_umask = umask(0000);
		result = mkdir(full_path, 0777);
		(void)umask(old_umask);
	}
	free(full_path);
	if (result != 0)
		return -1;

	// get the samba right, ftp right and target
	samba_right = DEFAULT_SAMBA_RIGHT;
	ftp_right = DEFAULT_FTP_RIGHT;

	len = strlen("*")+strlen(folder)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (!target)
		return -1;
	sprintf(target, "*%s=", folder);
	target[len] = 0;

	//  add folder's right to every var file
	get_account_list(&acc_num, &account_list);

	for (i = 0; i < acc_num; ++i) {
		// check if the created target is exist in the var file
		snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account_list[i]);
		var_info = read_whole_file(var_file);
		if (var_info == NULL) {
			;
		}
		else if (strstr(var_info, target) != NULL) {
			free(var_info);
			
			continue;
		}
		else
			free(var_info);
		
		// 7. add the folder's info in the var file
		fp = fopen(var_file, "a+");
		if (fp) {
			fprintf(fp, "%s%d%d%d\n", target, samba_right, ftp_right, DEFAULT_DMS_RIGHT);
			fclose(fp);
		}
	}

	free_2_dimension_list(&acc_num, &account_list);

	// get the var_file for anonymous ftp
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, FTP_ANONYMOUS_USER);
	fp = fopen(var_file, "a+");
	if (fp) {
		fprintf(fp, "%s%d%d%d\n", target, 1, 1, 1);
		fclose(fp);
	}

	free(target);

	// 9. add the folder's info in the folder list
	initial_folder_list_in_mount_path(mount_path);

	return 0;
}

int del_folder(const char *const mount_path, const char *const folder)
{
	int result, i, len;
	int acc_num = 0;
	char **account_list = NULL;
	char var_file[256];
	char *follow_info, backup;
	char *target, *var_info;
	FILE *fp;
	char *full_path;

	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;
	if (!is_valid_folder_name(folder))
		return -1;

	// 1. test if deleting the folder
	len = strlen(mount_path)+strlen("/")+strlen(folder);
	full_path = (char *)malloc(sizeof(char)*(len+1));
	if (!full_path)
		return -1;
	sprintf(full_path, "%s/%s", mount_path, folder);
	full_path[len] = 0;

	result = test_if_exist_folder_in_mount_path(mount_path, folder);
	if (result == 0) {
		result = test_if_dir(full_path);
		
		if (result != 1) {
			free(full_path);
			
			return -1;
		}
	}

	// 2. delete the folder
	result = rmdir(full_path);
	free(full_path);
	if (result != 0)
		return -1;

	// 4. get the target which is deleted in every var file
	len = strlen("*")+strlen(folder)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (!target)
		return -1;
	sprintf(target, "*%s=", folder);
	target[len] = 0;

	// 3. del folder's right to every var file
	get_account_list(&acc_num, &account_list);

	for (i = 0; i < acc_num; ++i) {
		// delete the content about the folder
		snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account_list[i]);
		var_info = read_whole_file(var_file);
		if (!var_info)
			continue;
		
		follow_info = upper_strstr(var_info, target);
		if (!follow_info) {
			free(var_info);
			continue;
		}
		backup = *follow_info;
		*follow_info = 0;
		
		fp = fopen(var_file, "w");
		if (!fp) {
			*follow_info = backup;
			free(var_info);
			continue;
		}
		fprintf(fp, "%s", var_info);
		
		*follow_info = backup;
		while (*follow_info != 0 && *follow_info != '\n')
			++follow_info;
		if (*follow_info != 0 && *(follow_info+1) != 0) {
			++follow_info;
			fprintf(fp, "%s", follow_info);
		}
		fclose(fp);
		
		free(var_info);
	}

	free_2_dimension_list(&acc_num, &account_list);

	// get the var_file for anonymous ftp
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, FTP_ANONYMOUS_USER);
	var_info = read_whole_file(var_file);
	if (var_info == NULL) {
		goto MOD_FOLDER_END;
	}

	follow_info = upper_strstr(var_info, target);
	if (follow_info == NULL) {
		free(var_info);
		
		goto MOD_FOLDER_END;
	}
	backup = *follow_info;
	*follow_info = 0;

	fp = fopen(var_file, "w");
	if (fp == NULL) {
		*follow_info = backup;
		free(var_info);
		
		goto MOD_FOLDER_END;
	}
	fprintf(fp, "%s", var_info);

	*follow_info = backup;
	while (*follow_info != 0 && *follow_info != '\n')
		++follow_info;
	if (*follow_info != 0 && *(follow_info+1) != 0) {
		++follow_info;
		fprintf(fp, "%s", follow_info);
	}
	fclose(fp);

	free(var_info);

MOD_FOLDER_END:

	free(target);

	// 9. modify the folder's info in the folder list
	initial_folder_list_in_mount_path(mount_path);

	return 0;
}

int mod_folder(const char *const mount_path, const char *const folder, const char *const new_folder)
{
	int result, i, len;
	int acc_num = 0;
	char **account_list = NULL;
	char var_file[256];
	char *target, *new_target, *var_info;
	FILE *fp;
	char *follow_info, backup;
	char *full_path, *new_full_path;
	struct stat target_stat;

	if (mount_path == NULL || strlen(mount_path) <= 0)
		return -1;
	if (!is_valid_folder_name(folder) || !is_valid_folder_name(new_folder))
		return -1;

	// 1. test if modifying the folder
	len = strlen(mount_path)+strlen("/")+strlen(folder);
	full_path = (char *)malloc(sizeof(char)*(len+1));
	if (!full_path)
		return -1;
	sprintf(full_path, "%s/%s", mount_path, folder);
	full_path[len] = 0;

	len = strlen(mount_path)+strlen("/")+strlen(new_folder);
	new_full_path = (char *)malloc(sizeof(char)*(len+1));
	if (!new_full_path)
		return -1;
	sprintf(new_full_path, "%s/%s", mount_path, new_folder);
	new_full_path[len] = 0;

	/* Never rename a file or follow a damaged folder-list state. */
	if (test_if_dir(full_path) != 1) {
		free(full_path);
		free(new_full_path);
		return -1;
	}
	result = test_if_exist_folder_in_mount_path(mount_path, folder);
	if (result < 0) {
		free(full_path);
		free(new_full_path);
		return -1;
	}
	if (result == 0) {
		/* The directory exists but is absent from the metadata list. */
		if (add_folder(mount_path, folder) != 0) {
			free(full_path);
			free(new_full_path);
			return -1;
		}
	}

	/* Renaming to an existing path can replace an empty directory or file.
	 * Treat an exact same-name request as a no-op and reject any other target
	 * that already exists, including folders absent from the metadata list. */
	if (strcmp(folder, new_folder) == 0) {
		free(full_path);
		free(new_full_path);
		return 0;
	}
	result = test_if_exist_folder_in_mount_path(mount_path, new_folder);
	if (result != 0 || lstat(new_full_path, &target_stat) == 0 ||
	    errno != ENOENT) {
		free(full_path);
		free(new_full_path);
		return -1;
	}

	// Modify the folder only when the target path is unused.
	result = rename(full_path, new_full_path);
	free(full_path);
	free(new_full_path);
	if (result != 0)
		return -1;

	len = strlen("*")+strlen(folder)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (!target)
		return -1;
	sprintf(target, "*%s=", folder);
	target[len] = 0;

	len = strlen("*")+strlen(new_folder)+strlen("=");
	new_target = (char *)malloc(sizeof(char)*(len+1));
	if (!new_target) {
		free(target);
		return -1;
	}
	sprintf(new_target, "*%s=", new_folder);
	new_target[len] = 0;

	// 3. add folder's right to every var file
	get_account_list(&acc_num, &account_list);

	for (i = 0; i < acc_num; ++i) {
		// check if the created target is exist in the var file
		snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, account_list[i]);
		var_info = read_whole_file(var_file);
		if (var_info == NULL) {
			continue;
		}
		
		if ((follow_info = strstr(var_info, target)) == NULL) {
			free(var_info);
			continue;
		}
		
		// 7. modify the folder's info in the var file
		fp = fopen(var_file, "w");
		if (fp) {
			// write the info before target
			backup = *follow_info;
			*follow_info = 0;
			fprintf(fp, "%s", var_info);
			*follow_info = backup;
			
			// write the info before new_target
			fprintf(fp, "%s", new_target);
			
			// write the info after target
			follow_info += strlen(target);
			fprintf(fp, "%s", follow_info);
			
			fclose(fp);
		}
		
		free(var_info);
	}

	free_2_dimension_list(&acc_num, &account_list);

	// get the var_file for anonymous ftp
	snprintf(var_file, sizeof(var_file), "%s/.__%s_var.txt", mount_path, FTP_ANONYMOUS_USER);
	var_info = read_whole_file(var_file);
	if (var_info) {
		if ((follow_info = strstr(var_info, target))) {
			fp = fopen(var_file, "w");
			if (fp) {
				// write the info before target
				backup = *follow_info;
				*follow_info = 0;
				fprintf(fp, "%s", var_info);
				*follow_info = backup;
				
				// write the info before new_target
				fprintf(fp, "%s", new_target);
				
				// write the info after target
				follow_info += strlen(target);
				fprintf(fp, "%s", follow_info);
				
				fclose(fp);
			}
		}
		
		free(var_info);
	}

	free(target);
	free(new_target);

	// 9. modify the folder's info in the folder list
	initial_folder_list_in_mount_path(mount_path);

	return 0;
}

int test_if_exist_account(const char *const account)
{
	int acc_num = 0;
	char **account_list = NULL;
	int result, i;

	if (!is_valid_account_name(account))
		return -1;
	if (!strcmp(account, FTP_ANONYMOUS_USER))
		return 1;

	if (get_account_list(&acc_num, &account_list) != 0)
		return -1;

	result = 0;
	for (i = 0; i < acc_num; ++i)
		if (!strcmp(account, account_list[i])) {
			result = 1;
			break;
		}

	free_2_dimension_list(&acc_num, &account_list);
	return result;
}

int test_if_exist_folder_in_mount_path(const char *const mount_path, const char *const folder)
{
	int sh_num = 0;
	char **folder_list = NULL;
	int result, i;

	if (!mount_path || !*mount_path || !is_valid_folder_name(folder))
		return -1;

	result = get_folder_list_in_mount_path(mount_path, &sh_num, &folder_list);
	if (result != 0) {
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}

	result = 0;
	for (i = 0; i < sh_num; ++i)
		if (!upper_strcmp(folder, folder_list[i])) {
			result = 1;
			break;
		}

	free_2_dimension_list(&sh_num, &folder_list);
	return result;
}

// for FTP: root dir is POOL_MOUNT_ROOT(/media)
int how_many_layer(int is_chroot, const char *basedir, char **mount_path, char **share)
{
	char *follow_info, *follow_info_end;
	size_t root_len, path_len, prefix_len, share_len;
	int layer = 0;

	if (!basedir || !mount_path || !share)
		return -1;
	*mount_path = NULL;
	*share = NULL;

	root_len = strlen(POOL_MOUNT_ROOT);
	path_len = strlen(basedir);
	if (!is_chroot &&
	    (path_len < root_len ||
	     strncmp(basedir, POOL_MOUNT_ROOT, root_len) != 0 ||
	     (basedir[root_len] != '\0' && basedir[root_len] != '/')))
		return -1;

	if (is_chroot)
		layer = BASE_LAYER;
	if (!strcmp(basedir, "/"))
		return layer;

	if (path_len > 1)
		layer++;

	follow_info = (char *)basedir;
	while (*follow_info != '\0' &&
	       (follow_info = index(follow_info + 1, '/')) != NULL)
		++layer;

	if (layer >= MOUNT_LAYER) {
		follow_info = (char *)basedir;
		if (!is_chroot)
			follow_info += root_len;
		/* For unchrooted paths, follow_info points at the slash after /media.
		 * For chrooted paths it points at the leading slash. */
		follow_info_end = index(follow_info + 1, '/');
		if (follow_info_end)
			prefix_len = (size_t)(follow_info_end - basedir);
		else
			prefix_len = path_len;
		if (layer >= SHARE_LAYER) {
			if (!follow_info_end)
				return -1;
			follow_info = follow_info_end;
		}

		*mount_path = (char *)malloc(prefix_len + 1);
		if (!*mount_path)
			return -1;
		memcpy(*mount_path, basedir, prefix_len);
		(*mount_path)[prefix_len] = '\0';
	}

	if (layer >= SHARE_LAYER) {
		if (!follow_info || *follow_info != '/') {
			free(*mount_path);
			*mount_path = NULL;
			return -1;
		}
		++follow_info;
		follow_info_end = index(follow_info, '/');
		share_len = follow_info_end ?
			(size_t)(follow_info_end - follow_info) : strlen(follow_info);
		if (share_len == 0) {
			free(*mount_path);
			*mount_path = NULL;
			return -1;
		}
		*share = (char *)malloc(share_len + 1);
		if (!*share) {
			free(*mount_path);
			*mount_path = NULL;
			return -1;
		}
		memcpy(*share, follow_info, share_len);
		(*share)[share_len] = '\0';
	}

	return layer;
}

