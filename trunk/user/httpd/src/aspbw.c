#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <httpd.h>

#include <shutils.h>

#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

// for backup =========================================================

int f_write(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode)
{
	const char nl = '\n';
	const char *data = (const char *)buffer;
	int f, total = 0, n;
	mode_t old_umask;

	if (!path || len < 0 || (len > 0 && !buffer))
		return -1;

	old_umask = umask(0);
	if (cmode == 0)
		cmode = 0666;
	f = open(path, (flags & FW_APPEND) ?
		 (O_WRONLY | O_CREAT | O_APPEND) :
		 (O_WRONLY | O_CREAT | O_TRUNC), cmode);
	if (f < 0) {
		umask(old_umask);
		return -1;
	}

	while (total < len) {
		n = write(f, data + total, (size_t)(len - total));
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0) {
			total = -1;
			break;
		}
		total += n;
	}

	if (total >= 0 && (flags & FW_NEWLINE)) {
		do {
			n = write(f, &nl, 1);
		} while (n < 0 && errno == EINTR);
		if (n != 1)
			total = -1;
		else
			total++;
	}

	if (close(f) != 0 && total >= 0)
		total = -1;
	umask(old_umask);
	return total;
}

// for bandwidth =========================================================

int f_exists(const char *path)	// note: anything but a directory
{
	struct stat st;
	return (stat(path, &st) == 0) && (!S_ISDIR(st.st_mode));
}

static int _f_wait_exists(const char *name, int max, int invert)
{
	max *= 40;
	while (max-- > 0) {
		if (f_exists(name) ^ invert)
			return 1;
		usleep(25000);
	}
	return 0;
}

int f_wait_exists(const char *name, int max)
{
	return _f_wait_exists(name, max, 0);
}

int f_read(const char *path, void *buffer, int max)
{
	int f, n;

	if (!path || !buffer || max < 0)
		return -1;
	if ((f = open(path, O_RDONLY)) < 0)
		return -1;

	do {
		n = read(f, buffer, (size_t)max);
	} while (n < 0 && errno == EINTR);

	if (close(f) != 0 && n >= 0)
		return -1;
	return n;
}

int f_read_string(const char *path, char *buffer, int max)
{
	int n;

	if (!path || !buffer || max <= 0)
		return -1;
	n = f_read(path, buffer, max - 1);
	if (n < 0) {
		buffer[0] = '\0';
		return -1;
	}
	buffer[n] = '\0';
	return n;
}

size_t strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len, ret;

	if (!s)
		return 0;
	len = strlen(s);
	ret = len;
	if (bufsize == 0)
		return ret;
	if (!d)
		return 0;
	if (len >= bufsize)
		len = bufsize - 1;
	memcpy(d, s, len);
	d[len] = '\0';
	return ret;
}

/**
 * hex char to int
 * @author 荒野无灯
 * @date 2016-08-01 12:30
 * @param hex
 * @return int
 */
static int
hex_nibble(uint8_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static int
hex_byte_at(const uint8_t *hex)
{
	int hi, lo;

	if (!hex)
		return -1;
	hi = hex_nibble(hex[0]);
	lo = hex_nibble(hex[1]);
	if (hi < 0 || lo < 0)
		return -1;
	return (hi << 4) | lo;
}

static int
is_ssid_plain_char(uint8_t c)
{
	return (c >= '0' && c <= '9') ||
	       (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       c == '!' || c == '*' || c == '(' || c == ')' ||
	       c == '_' || c == '-' || c == '\'' || c == '.';
}

static int
is_valid_hex_string(const uint8_t *input)
{
	size_t input_len, payload_len, i;
	int c, c2, c3;

	if (!input)
		return 0;
	input_len = strlen((const char *)input);
	if (input_len <= 4 || (input_len & 1) ||
	    input[0] != '0' || input[1] != 'x')
		return 0;

	payload_len = (input_len - 2) / 2;
	for (i = 0; i < payload_len;) {
		c = hex_byte_at(input + 2 + i * 2);
		if (c < 0)
			return 0;
		if (is_ssid_plain_char((uint8_t)c)) {
			i++;
			continue;
		}
		if (c >= 0xe4 && c <= 0xe9 && i + 2 < payload_len) {
			c2 = hex_byte_at(input + 2 + (i + 1) * 2);
			c3 = hex_byte_at(input + 2 + (i + 2) * 2);
			if (c2 >= 0x80 && c2 <= 0xbf &&
			    c3 >= 0x80 && c3 <= 0xbf) {
				i += 3;
				continue;
			}
		}
		return 0;
	}
	return 1;
}

void
char_to_ascii(char *output, size_t output_size, const uint8_t *input)
{
	static const char hex_digits[] = "0123456789ABCDEF";
	size_t input_len, required = 0, i, out = 0;
	int use_hex;

	if (!output || output_size == 0)
		return;
	output[0] = '\0';
	if (!input)
		return;

	input_len = strlen((const char *)input);
	use_hex = is_valid_hex_string(input);
	if (use_hex) {
		size_t payload_len = (input_len - 2) / 2;
		if (payload_len > (output_size - 1) / 3)
			return;
		for (i = 2; i < input_len; i += 2) {
			output[out++] = '%';
			output[out++] = (char)input[i];
			output[out++] = (char)input[i + 1];
		}
		output[out] = '\0';
		return;
	}

	for (i = 0; i < input_len; i++) {
		size_t add = is_ssid_plain_char(input[i]) ? 1 : 3;
		if (add > output_size - 1 || required > output_size - 1 - add)
			return;
		required += add;
	}
	for (i = 0; i < input_len; i++) {
		uint8_t c = input[i];
		if (is_ssid_plain_char(c)) {
			output[out++] = (char)c;
		} else {
			output[out++] = '%';
			output[out++] = hex_digits[c >> 4];
			output[out++] = hex_digits[c & 0x0f];
		}
	}
	output[out] = '\0';
}

int do_f(const char *path, webs_t wp)
{
	FILE *fp;
	char buf[1024];
	int ret = 0;

	fp = fopen(path, "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp))
			ret += fputs(buf, wp);
		fclose(fp);
	} else {
		ret += fputs("", wp);
	}

	fflush(wp);

	return ret;
}

