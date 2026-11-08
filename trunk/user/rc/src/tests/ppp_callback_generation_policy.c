#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ppp_callback_generation_policy.h"

int
main(void)
{
	unsigned long generation, next;
	char text[sizeof(unsigned long) * CHAR_BIT + 1];

	generation = 0;
	assert(ppp_callback_generation_parse("1", &generation) == 0);
	assert(generation == 1);
	assert(ppp_callback_generation_matches("1", 1));
	assert(!ppp_callback_generation_matches("1", 2));
	assert(!ppp_callback_generation_matches("2", 1));

	assert(ppp_callback_generation_next(0, &next) == 0 && next == 1);
	assert(ppp_callback_generation_next(41, &next) == 0 && next == 42);
	assert(ppp_callback_generation_next(ULONG_MAX, &next) == -1);
	assert(errno == ERANGE);

	assert(ppp_callback_generation_format(42, text, sizeof(text)) == 0);
	assert(strcmp(text, "42") == 0);
	assert(ppp_callback_generation_parse(text, &generation) == 0);
	assert(generation == 42);
	assert(ppp_callback_generation_format(1, text, 1) == -1);
	assert(errno == EOVERFLOW);
	assert(text[0] == '\0');

	assert(ppp_callback_generation_parse(NULL, &generation) == -1);
	assert(errno == EINVAL);
	assert(ppp_callback_generation_parse("", &generation) == -1);
	assert(ppp_callback_generation_parse("0", &generation) == -1);
	assert(ppp_callback_generation_parse("01", &generation) == -1);
	assert(ppp_callback_generation_parse("+1", &generation) == -1);
	assert(ppp_callback_generation_parse("-1", &generation) == -1);
	assert(ppp_callback_generation_parse("1x", &generation) == -1);
	assert(ppp_callback_generation_parse(" 1", &generation) == -1);
	assert(ppp_callback_generation_parse("999999999999999999999999999999999999999999", &generation) == -1);
	assert(!ppp_callback_generation_matches("01", 1));
	assert(!ppp_callback_generation_matches("1", 0));

	/* A stop advances the generation; restart advances it again. A callback
	 * captured before either boundary must never match the active session. */
	assert(ppp_callback_generation_next(1, &next) == 0 && next == 2);
	assert(!ppp_callback_generation_matches("1", next));
	assert(ppp_callback_generation_next(next, &generation) == 0);
	assert(!ppp_callback_generation_matches("1", generation));
	assert(ppp_callback_generation_matches("3", generation));

	puts("PPP callback generation fencing regression: OK");
	return 0;
}
