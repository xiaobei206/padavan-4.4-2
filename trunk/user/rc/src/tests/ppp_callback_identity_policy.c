#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ppp_callback_identity_policy.h"

static void
expect_valid(const char *value, const char *prefix, unsigned int max_units,
             int expected)
{
	int index = -99;

	assert(ppp_callback_parse_index(value, prefix, max_units, &index) == 0);
	assert(index == expected);
}

static void
expect_invalid(const char *value, const char *prefix, unsigned int max_units)
{
	int index = 123;

	assert(ppp_callback_parse_index(value, prefix, max_units, &index) == -1);
	assert(index == -1);
}

int
main(void)
{
	expect_valid("ppp0", "ppp", 8, 0);
	expect_valid("ppp7", "ppp", 8, 7);
	expect_valid("wan0", "wan", 1, 0);

	expect_invalid(NULL, "ppp", 8);
	expect_invalid("ppp0", NULL, 8);
	expect_invalid("ppp", "ppp", 8);
	expect_invalid("pppx", "ppp", 8);
	expect_invalid("ppp-1", "ppp", 8);
	expect_invalid("ppp+1", "ppp", 8);
	expect_invalid("ppp00", "ppp", 8);
	expect_invalid("ppp0junk", "ppp", 8);
	expect_invalid("ppp8", "ppp", 8);
	expect_invalid("ppp999999999999999999999999", "ppp", 8);
	expect_invalid("wan1", "wan", 1);
	expect_invalid("wan0garbage", "wan", 1);
	expect_invalid("wan00", "wan", 1);
	expect_invalid("WAN0", "wan", 1);
	expect_invalid("wan", "wan", 1);
	expect_invalid("wan0", "wan", 0);
	expect_invalid("ppp0", "ppp", 0);

	puts("PPP callback identity regression: OK");
	return 0;
}
