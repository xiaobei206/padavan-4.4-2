#ifndef PADAVAN_PPP_OPTION_VALUE_POLICY_H
#define PADAVAN_PPP_OPTION_VALUE_POLICY_H

#include <errno.h>
#include <stddef.h>
#include <string.h>

/*
 * Escape values written into pppd's single-quoted option syntax.
 * Reject control characters so one configured value cannot turn into multiple
 * option lines. Custom option text has a separate mode that preserves LF/TAB.
 * Both modes fail on overflow instead of returning raw or truncated input.
 */
static int
ppp_option_escape_internal(const char *value, char *out, size_t out_size,
                           int allow_line_breaks)
{
	const unsigned char *src;
	size_t needed, extra, used;

	if (!out || out_size == 0) {
		errno = EINVAL;
		return -1;
	}
	out[0] = '\0';
	if (!value) {
		errno = EINVAL;
		return -1;
	}

	needed = 1; /* terminating NUL */
	for (src = (const unsigned char *)value; *src; ++src) {
		if (*src < 0x20 &&
		    !(allow_line_breaks && (*src == '\n' || *src == '\t'))) {
			errno = EINVAL;
			return -1;
		}
		if (*src == 0x7f) {
			errno = EINVAL;
			return -1;
		}

		extra = (*src == '\'' || *src == '\\') ? 2U : 1U;
		if (needed > (size_t)-1 - extra) {
			errno = EOVERFLOW;
			return -1;
		}
		needed += extra;
	}

	if (needed > out_size) {
		errno = EOVERFLOW;
		return -1;
	}

	used = 0;
	for (src = (const unsigned char *)value; *src; ++src) {
		if (*src == '\'' || *src == '\\')
			out[used++] = '\\';
		out[used++] = (char)*src;
	}
	out[used] = '\0';

	return 0;
}

static int
ppp_option_value_escape(const char *value, char *out, size_t out_size)
{
	return ppp_option_escape_internal(value, out, out_size, 0);
}

static int
ppp_option_text_escape(const char *value, char *out, size_t out_size)
{
	return ppp_option_escape_internal(value, out, out_size, 1);
}

#endif /* PADAVAN_PPP_OPTION_VALUE_POLICY_H */
