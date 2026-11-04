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
#define __USE_GNU
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>

#include <image.h>

#include "common.h"
#include "httpd.h"

#ifndef O_BINARY
#define O_BINARY	0
#endif

#define IMAGE_HEADER		"HDR0"
#define PROFILE_HEADER		"HDR1"
#define PROFILE_HEADER_NEW	"HDR2"
#define UPLOAD_BUF_SIZE		1024

typedef int (*check_header_t)(const char *, long *);

static int
check_header_bz2(const char *buf, long *file_len)
{
	const unsigned char bz2h_1[3] = { 0x42, 0x5a, 0x68 };
	const unsigned char bz2h_2[6] = { 0x31, 0x41, 0x59, 0x26, 0x53, 0x59 };

	if (memcmp(buf, bz2h_1, 3) == 0 && memcmp(buf+4, bz2h_2, 6) == 0)
		return 0;

	httpd_log("%s: Incorrect %s header!", "Storage restore", "BZ2");

	return -1;
}

static int
check_header_nvram(const char *buf, long *file_len)
{
	unsigned int raw_len;
	memcpy(&raw_len, buf + 4, sizeof(raw_len));

	if (strncmp(buf, PROFILE_HEADER_NEW, 4) == 0) {
		*file_len = (long)(raw_len & 0xffffff);
		return 0;
	}

	if (strncmp(buf, PROFILE_HEADER, 4) == 0) {
		*file_len = (long)raw_len;
		return 0;
	}

	httpd_log("%s: Incorrect %s header!", "NVRAM restore", "profile");

	return -1;
}

static int
check_header_image(const char *buf, long *file_len)
{
	int pid_asus_len;
	char pid_asus[16];
	image_header_t *hdr = (image_header_t *)buf;

	/* check header magic */
	if (ntohl(hdr->ih_magic) != IH_MAGIC) {
		httpd_log("%s: Incorrect %s header!", "Firmware update", "image");
		return -1;
	}

	pid_asus_len = strlen(BOARD_PID);
	if (pid_asus_len > 12)
		pid_asus_len = 12;

	strncpy(pid_asus, buf+36, pid_asus_len);
	pid_asus[pid_asus_len] = 0;

	if (strcmp(pid_asus, BOARD_PID) != 0) {
		httpd_log("%s: Incorrect image ProductID: %s! Expected is %s.", "Firmware update", pid_asus, BOARD_PID);
		return -2;
	}

	*file_len = (long)(ntohl(hdr->ih_size) + sizeof(image_header_t));

	return 0;
}

static int
check_crc_image(const char *fw_image)
{
	int ifd;
	uint32_t checksum, datalen;
	struct stat sbuf;
	unsigned char *ptr = (unsigned char *)MAP_FAILED;
	image_header_t header2;
	image_header_t *hdr, *hdr2=&header2;
	int ret = 0;

	ifd = open(fw_image, O_RDONLY|O_BINARY);
	if (ifd < 0)
		return -1;

	/* We're a bit of paranoid */
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (fstat(ifd, &sbuf) < 0) {
		ret=-1;
		goto checkcrc_fail;
	}

	if ((unsigned int)sbuf.st_size < (sizeof(image_header_t) + (2 * 1024 * 1024)) || 
	    (unsigned int)sbuf.st_size > get_mtd_size(FW_MTD_NAME)) {
		ret = -1;
		httpd_log("%s: Firmware image size is invalid!", "Firmware update");
		goto checkcrc_fail;
	}

	ptr = (unsigned char *)mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, ifd, 0);
	if (ptr == (unsigned char *)MAP_FAILED) {
		ret = -1;
		httpd_log("%s: Unable to mmap firmware image!", "Firmware update");
		goto checkcrc_fail;
	}

	hdr = (image_header_t *)ptr;

	memcpy(hdr2, hdr, sizeof(image_header_t));
	memset(&hdr2->ih_hcrc, 0, sizeof(uint32_t));
	checksum = crc32_sp(0, (const char *)hdr2, sizeof(image_header_t));

	if (checksum != ntohl(hdr->ih_hcrc)) {
		ret = -1;
		httpd_log("%s: Firmware image %s has invalid CRC!", "Firmware update", "header");
		goto checkcrc_fail;
	}

	datalen = ntohl(hdr->ih_size);
	if (datalen > ((unsigned int)sbuf.st_size - sizeof(image_header_t))) {
		ret = -1;
		httpd_log("%s: Firmware image is corrupted! Please check free space in /tmp!", "Firmware update");
		goto checkcrc_fail;
	}

	checksum = crc32_sp(0, (const char *)(ptr + sizeof(image_header_t)), datalen);
	if (checksum != ntohl(hdr->ih_dcrc)) {
		ret = -1;
		httpd_log("%s: Firmware image %s has invalid CRC!", "Firmware update", "body");
		goto checkcrc_fail;
	}

checkcrc_fail:
	if (ptr != (unsigned char *)MAP_FAILED)
		munmap((void *)ptr, sbuf.st_size);

#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	close(ifd);

	return ret;
}

static int
do_upload_file(FILE *stream, int clen, char *bndr, const char *fn, const char *obj_name, check_header_t func_hdr, int hdr_size)
{
	FILE *fp = NULL;
	char buf[64+UPLOAD_BUF_SIZE+1], buf_obj[32], *ptr;
	int cnt, count, offset, ret, ch, valid_header, boundary_found;
	long filelen, end_pos, back_len;

	if (!stream || !fn || !*fn || !obj_name || !func_hdr ||
	    clen < 0 || hdr_size <= 0 || hdr_size > UPLOAD_BUF_SIZE)
		return -1;

	ret = EINVAL;
	valid_header = 0;
	boundary_found = (bndr == NULL);
	snprintf(buf_obj, sizeof(buf_obj), "name=\"%s\"", obj_name);

	/* Look for our part */
	while (clen > 0) {
		if (!fgets(buf, MIN(clen, (int)sizeof(buf) - 1) + 1, stream))
			goto err;
		clen -= strlen(buf);
		if (!strncasecmp(buf, "Content-Disposition:", 20) && strstr(buf, buf_obj))
			break;
	}

	/* Skip boundary and headers */
	while (clen > 0) {
		if (!fgets(buf, MIN(clen, (int)sizeof(buf) - 1) + 1, stream))
			goto err;
		clen -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n"))
			break;
	}

	unlink(fn);
	if (!(fp = fopen(fn, "w+")))
		goto err;

	filelen = clen;
	offset = 0;
	cnt = 0;
	ptr = buf + 64;
	memset(buf, 0, 64);

	/* Pipe the rest to the FIFO */
	while (clen > 0 && filelen > 0) {
		count = fread(ptr + offset, 1, MIN(clen, UPLOAD_BUF_SIZE-offset), stream);
		if (count <= 0)
			goto err;
		
		clen -= count;
		
		if (cnt == 0) {
			if (count + offset < hdr_size) {
				offset += count;
				continue;
			}
			
			count += offset;
			offset = 0;
			cnt++;
			
			if (func_hdr(ptr, &filelen) != 0)
				goto err;
			
			valid_header = 1;
		}
		
		/* check boundary marker (after \r\n or \n) */
		if (bndr) {
			char *pb = memmem(buf, 64 + count, bndr, strlen(bndr));
			if (!pb)
				pb = memmem(buf, 64 + count, bndr+1, strlen(bndr+1));
			if (pb) {
				boundary_found = 1;
				if (pb >= ptr) {
					count = (int)(pb - ptr);
					filelen = count;
				} else {
					/* The marker began in the previous chunk; remove its written prefix. */
					back_len = (long)(ptr - pb);
					if (fflush(fp) != 0 ||
					    (end_pos = ftell(fp)) < back_len ||
					    ftruncate(fileno(fp), (off_t)(end_pos - back_len)) != 0 ||
					    fseek(fp, end_pos - back_len, SEEK_SET) != 0)
						goto err;
					filelen = 0;
					break;
				}
			}
		}
		
		if (count > filelen)
			count = filelen;
		
		filelen -= count;
		if (count > 0 && fwrite(ptr, 1, (size_t)count, fp) != (size_t)count)
			goto err;

		if (bndr) {
			if (count >= 64) {
				memcpy(buf, ptr + count - 64, 64);
			} else if (count > 0) {
				memmove(buf, buf + count, 64 - count);
				memcpy(buf + 64 - count, ptr, (size_t)count);
			}
		}
	}

	if (!valid_header || filelen > 0 || (bndr && !boundary_found))
		goto err;

	/* Slurp anything remaining in the request */
	while (clen-- > 0) {
		if((ch = fgetc(stream)) == EOF)
			break;
		
		if (filelen > 0) {
			if (fwrite(&ch, 1, 1, fp) != 1)
				goto err;
			filelen--;
		}
	}

	ret = 0;

err:
	if (fp) {
		if (fclose(fp) != 0 && ret == 0)
			ret = EIO;
		fp = NULL;
	}

	/* Slurp anything remaining in the request */
	while (clen-- > 0) {
		if((ch = fgetc(stream)) == EOF)
			break;
	}

	if (ret != 0 || !valid_header) {
		unlink(fn);
		return -1;
	}

	return 0;
}

void
do_upgrade_fw_post(const char *url, FILE *stream, int clen, char *boundary)
{
	const char *upload_file = FW_IMG_NAME;
	int ret;

	/* delete some files (need free space in /tmp) */
	unlink("/tmp/usb.log");
	unlink("/tmp/syscmd.log");
	doSystem("rm -rf %s", "/tmp/xupnpd-cache");
	doSystem("rm -rf %s", "/tmp/xupnpd-feeds");

	/* copy mtd_write to RAM */
	doSystem("cp -f %s %s", "/bin/mtd_write", "/tmp");

	/* reclaim RAM from caches */
	fput_int("/proc/sys/vm/drop_caches", 1);

	ret = do_upload_file(stream, clen, NULL, upload_file, "file", check_header_image, sizeof(image_header_t));
	if (ret == 0) {
		ret = check_crc_image(upload_file);
		if (ret != 0)
			unlink(upload_file);
	}
}

void
do_restore_nv_post(const char *url, FILE *stream, int clen, char *boundary)
{
	const char *upload_file = PROFILE_FIFO_UPLOAD;

	do_upload_file(stream, clen, NULL, upload_file, "file_nv", check_header_nvram, 8);
}

void
do_restore_st_post(const char *url, FILE *stream, int clen, char *boundary)
{
	const char *upload_file = STORAGE_FIFO_FILENAME;
	char bndr[128];
	const unsigned char *p;
	int bndr_len;

	if (!boundary || !*boundary || clen < 0)
		return;
	for (p = (const unsigned char *)boundary; *p; ++p) {
		if (*p < 32 || *p > 126)
			return;
	}

	/* BZ2 does not store file length in its header; require a valid MIME boundary. */
	bndr_len = snprintf(bndr, sizeof(bndr), "\r\n--%s", boundary);
	if (bndr_len < 0 || bndr_len >= (int)sizeof(bndr))
		return;
	do_upload_file(stream, clen, bndr, upload_file, "file_st", check_header_bz2, 10);
}
