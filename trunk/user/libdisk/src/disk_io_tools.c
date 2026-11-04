/*
 * This program is free software; you can redistribute it and/or
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
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "disk_io_tools.h"

void sanity_name(char *name)
{
	int len, i;
	if (!name)
		return;
	len = strlen(name);

	for (i = 0; i < len; i++)
	{
		if (name[i] == 0x22 ||
		    name[i] == 0x23 ||
		    name[i] == 0x25 ||
		    name[i] == 0x27 ||
		    name[i] == 0x2F ||
		    name[i] == 0x5C ||
		    name[i] == 0x60 ||
		    name[i] == 0x84)
			name[i] = 0x20;
	}
}

char *read_whole_file(const char *target)
{
	FILE *fp;
	char *buffer = NULL, *new_buffer;
	size_t used = 0, capacity = 0, next_capacity, n;
	int ch;

	/* These are small metadata/configuration files, not arbitrary payloads.
	 * Cap reads to avoid a malformed removable-drive file exhausting RAM. */
#define READ_WHOLE_FILE_MAX (1024U * 1024U)
	if (!target || !*target)
		return NULL;
	fp = fopen(target, "r");
	if (!fp)
		return NULL;

	for (;;) {
		if (capacity - used < 2) {
			if (capacity >= (size_t)READ_WHOLE_FILE_MAX + 1) {
				ch = fgetc(fp);
				if (ch != EOF) {
					free(buffer);
					fclose(fp);
					return NULL;
				}
				if (ferror(fp)) {
					free(buffer);
					fclose(fp);
					return NULL;
				}
				break;
			}

			next_capacity = capacity ? capacity * 2 : 1024;
			if (next_capacity > (size_t)READ_WHOLE_FILE_MAX + 1)
				next_capacity = (size_t)READ_WHOLE_FILE_MAX + 1;
			new_buffer = realloc(buffer, next_capacity);
			if (!new_buffer) {
				free(buffer);
				fclose(fp);
				return NULL;
			}
			buffer = new_buffer;
			capacity = next_capacity;
		}

		n = fread(buffer + used, 1, capacity - used - 1, fp);
		used += n;
		if (n > 0)
			continue;
		if (ferror(fp)) {
			free(buffer);
			fclose(fp);
			return NULL;
		}
		break;
	}

	if (!buffer) {
		buffer = malloc(1);
		if (!buffer) {
			fclose(fp);
			return NULL;
		}
	}
	buffer[used] = '\0';
	fclose(fp);
	return buffer;
#undef READ_WHOLE_FILE_MAX
}

int mkdir_if_none(char *dir) {
	DIR *dp;
	mode_t old_umask;
	int result;

	if (!dir || !*dir)
		return 0;
	dp = opendir(dir);
	if (dp != NULL) {
		closedir(dp);
		return 0;
	}

	old_umask = umask(0000);
	result = mkdir(dir, 0777);
	(void)umask(old_umask);
	return result == 0 ? 1 : 0;
}

int delete_file_or_dir(char *target) {
	if (test_if_dir(target))
		rmdir(target);
	else
		unlink(target);
	
	return 0;
}

int test_if_file(const char *file) {
	FILE *fp = fopen(file, "r");
	
	if (fp == NULL)
		return 0;
	
	fclose(fp);
	return 1;
}

int test_if_dir(const char *dir) {
	DIR *dp = opendir(dir);
	
	if (dp == NULL)
		return 0;
	
	closedir(dp);
	return 1;
}

int test_if_System_folder(const char *const dirname) {
	char *MS_System_folder[] = {"SYSTEM VOLUME INFORMATION", "RECYCLER", "RECYCLED", "$RECYCLE.BIN", NULL};
	char *Linux_System_folder[] = {"lost+found", "opt", "aria", "transmission", NULL};
	int i;
	
	for (i = 0; MS_System_folder[i] != NULL; ++i) {
		if (!upper_strcmp(dirname, MS_System_folder[i]))
			return 1;
	}
	
	for (i = 0; Linux_System_folder[i] != NULL; ++i) {
		if (!upper_strcmp(dirname, Linux_System_folder[i]))
			return 1;
	}
	
	return 0;
}

int test_mounted_disk_size_status(char *diskpath) {
	struct statfs fsbuf;
	unsigned long long block_size;

	if (statfs(diskpath, &fsbuf)) {
		perror("*** statfs fail! - in test_mounted_disk_size_status()");
		return 0;
	}
	
	block_size = fsbuf.f_bsize;
	if (block_size*fsbuf.f_bfree/(1<<20) < (unsigned long long)33)
		return 1;
	else if (block_size*fsbuf.f_blocks/(1<<20) > (unsigned long long)256)
		return 2;
	else
		return 3;
}

char *get_upper_str(const char *const str) {
	int len, i;
	char *target;

	if(str == NULL || strlen(str) <= 0)
		return NULL;

	len = strlen(str);
	target = (char *)malloc(sizeof(char)*(len+1));
	if (target == NULL)
		return NULL;
	for (i = 0; i < len; ++i)
		target[i] = (char)toupper((unsigned char)str[i]);
	target[len] = 0;
	
	return target;
}

int upper_strcmp(const char *const str1, const char *const str2) {
	int len1, len2, i;

	if(str1 == NULL || strlen(str1) <= 0
			|| str2 == NULL || strlen(str2) <= 0)
		return -1;

	len1 = strlen(str1);
	len2 = strlen(str2);
	if (len1 != len2)
		return len1-len2;
	
	for (i = 0; i < len1; ++i) {
		if (toupper((unsigned char)str1[i]) != toupper((unsigned char)str2[i]))
			return i+1;
	}
	
	return 0;
}

int upper_strncmp(const char *const str1, const char *const str2, int max_len)
{
	int i;

	if (!str1 || !str2 || max_len <= 0)
		return -1;

	for (i = 0; i < max_len; ++i) {
		unsigned char c1 = (unsigned char)str1[i];
		unsigned char c2 = (unsigned char)str2[i];

		if (c1 == '\0' || c2 == '\0')
			return c1 == c2 ? 0 : i + 1;
		if (toupper(c1) != toupper(c2))
			return i + 1;
	}

	return 0;
}

char *upper_strstr(const char *const str1, const char *const str2)
{
	char *line, *line_end, *line_str;
	int len, ret, search_len;

	if (!str1 || !*str1 || !str2 || !*str2)
		return NULL;

	search_len = strlen(str2);
	line = (char *)str1;
	while (line) {
		line_end = strchr(line, '\n');
		len = line_end ? (int)(line_end - line) : (int)strlen(line);

		line_str = malloc((size_t)len + 1);
		if (!line_str)
			return NULL;
		memcpy(line_str, line, (size_t)len);
		line_str[len] = '\0';

		ret = upper_strncmp(line_str, str2, search_len);
		free(line_str);
		if (ret == 0)
			return line;

		/* A final line need not end in newline. */
		if (!line_end)
			break;
		line = strchr(line_end + 1, '*');
	}

	return NULL;
}

void strntrim(char *str)
{
	size_t begin = 0, end;

	if (!str)
		return;
	end = strlen(str);
	while (begin < end && isspace((unsigned char)str[begin]))
		++begin;
	while (end > begin && isspace((unsigned char)str[end - 1]))
		--end;

	if (begin > 0 && end > begin)
		memmove(str, str + begin, end - begin);
	str[end - begin] = '\0';
}

void write_escaped_value(FILE *fp, const char *value) {
	const char *follow_value;

	if (!fp || !value)
		return;
	follow_value = value;
	while (*follow_value != 0) {
		if (*follow_value == '\'') {
			fputc('\\', fp);
			fputc('\'', fp);
		}
		else if (*follow_value == '\"') {
			fputc('\\', fp);
			fputc('\"', fp);
		}
		else if (*follow_value == '\?') {
			fputc('\\', fp);
			fputc('\?', fp);
		}
		else if (*follow_value == '\\') {
			fputc('\\', fp);
			fputc('\\', fp);
		}
		else if (*follow_value == '\a') {
			fputc('\\', fp);
			fputc('a', fp);
		}
		else if (*follow_value == '\b') {
			fputc('\\', fp);
			fputc('b', fp);
		}
		else if (*follow_value == '\f') {
			fputc('\\', fp);
			fputc('f', fp);
		}
		else if (*follow_value == '\n') {
			fputc('\\', fp);
			fputc('n', fp);
		}
		else if (*follow_value == '\r') {
			fputc('\\', fp);
			fputc('r', fp);
		}
		else if (*follow_value == '\t') {
			fputc('\\', fp);
			fputc('t', fp);
		}
		else if (*follow_value == '\v') {
			fputc('\\', fp);
			fputc('v', fp);
		}
		else if (isprint((unsigned char)*follow_value)) {
			fputc(*follow_value, fp);
		}
		else if (((unsigned char)(*follow_value) & 0x80) != 0) {
			fputc(*follow_value, fp);
		}
		else{
			fprintf(fp, "\\%03o", (unsigned)(unsigned char)(*follow_value));
		}
		++follow_value;
	}
}
