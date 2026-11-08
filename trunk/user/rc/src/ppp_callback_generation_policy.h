#ifndef PADAVAN_PPP_CALLBACK_GENERATION_POLICY_H
#define PADAVAN_PPP_CALLBACK_GENERATION_POLICY_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>

/* Parse a canonical, positive decimal session generation without atoi()
 * truncation, signs, whitespace, leading zeroes or integer overflow. */
static int
ppp_callback_generation_parse(const char *text, unsigned long *generation_out)
{
	const unsigned char *p;
	unsigned long generation, digit;

	if (generation_out)
		*generation_out = 0;
	if (!text || !text[0] || !generation_out) {
		errno = EINVAL;
		return -1;
	}
	p = (const unsigned char *)text;
	if (*p == '0' && p[1] != '\0') {
		errno = EINVAL;
		return -1;
	}
	generation = 0;
	for (; *p; ++p) {
		if (*p < '0' || *p > '9') {
			errno = EINVAL;
			return -1;
		}
		digit = (unsigned long)(*p - '0');
		if (generation > (ULONG_MAX - digit) / 10UL) {
			errno = ERANGE;
			return -1;
		}
		generation = generation * 10UL + digit;
	}
	if (generation == 0) {
		errno = EINVAL;
		return -1;
	}
	*generation_out = generation;
	return 0;
}

static int
ppp_callback_generation_initial_state_allowed(int created, int allow_empty)
{
	/* Empty is valid only for the process that atomically created the file.
	 * A truncated/corrupt pre-existing file must never reset the token. */
	return created && allow_empty;
}

static int
ppp_callback_generation_matches(const char *text, unsigned long current)
{
	unsigned long supplied;

	if (current == 0 || ppp_callback_generation_parse(text, &supplied) != 0)
		return 0;
	return supplied == current;
}

static int
ppp_callback_generation_next(unsigned long current, unsigned long *next_out)
{
	if (!next_out || current == ULONG_MAX) {
		errno = current == ULONG_MAX ? ERANGE : EINVAL;
		return -1;
	}
	*next_out = current + 1UL;
	return 0;
}

static int
ppp_callback_generation_format(unsigned long generation, char *out,
			       size_t out_size)
{
	char reversed[sizeof(unsigned long) * CHAR_BIT + 1];
	size_t used;
	unsigned long value;

	if (!out || out_size == 0 || generation == 0) {
		errno = EINVAL;
		return -1;
	}
	used = 0;
	value = generation;
	while (value > 0) {
		reversed[used++] = (char)('0' + (value % 10UL));
		value /= 10UL;
	}
	if (used + 1 > out_size) {
		out[0] = '\0';
		errno = EOVERFLOW;
		return -1;
	}
	{
		size_t i;
		for (i = 0; i < used; ++i)
			out[i] = reversed[used - i - 1];
	}
	out[used] = '\0';
	return 0;
}

#endif /* PADAVAN_PPP_CALLBACK_GENERATION_POLICY_H */
