#ifndef PADAVAN_PPP_CALLBACK_IDENTITY_POLICY_H
#define PADAVAN_PPP_CALLBACK_IDENTITY_POLICY_H

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * Parse exact <prefix><decimal-unit> callback identities. The caller supplies
 * an exclusive upper bound because accepted PPP units and logical WAN units
 * have different ranges. Reject suffixes, signs, empty numbers, leading zeroes
 * and overflow rather than trusting atoi()'s partial parse.
 */
static int
ppp_callback_parse_index(const char *value, const char *prefix,
                         unsigned int max_index_exclusive, int *index_out)
{
	const unsigned char *p;
	size_t prefix_len;
	unsigned int parsed, digit, max_value;

	if (index_out)
		*index_out = -1;
	if (!value || !prefix || !index_out || max_index_exclusive == 0 ||
	    max_index_exclusive > (unsigned int)INT_MAX)
		return -1;

	prefix_len = strlen(prefix);
	if (prefix_len == 0 || strncmp(value, prefix, prefix_len) != 0)
		return -1;

	p = (const unsigned char *)value + prefix_len;
	if (*p < '0' || *p > '9')
		return -1;
	if (*p == '0' && p[1] != '\0')
		return -1;

	max_value = max_index_exclusive - 1;
	parsed = 0;
	for (; *p; ++p) {
		if (*p < '0' || *p > '9')
			return -1;

		digit = (unsigned int)(*p - '0');
		if (digit > max_value || parsed > (max_value - digit) / 10U)
			return -1;
		parsed = parsed * 10U + digit;
	}

	*index_out = (int)parsed;
	return 0;
}

#endif /* PADAVAN_PPP_CALLBACK_IDENTITY_POLICY_H */
