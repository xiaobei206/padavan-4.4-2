#include <assert.h>
#include <string.h>

#include "httpd_basic_auth.h"

static void
expect_decode_failure(const char *encoded, int capacity)
{
	unsigned char output[16];
	int i;

	memset(output, 0xa5, sizeof(output));
	assert(b64_decode_strict(encoded, output, capacity) < 0);
	for (i = 0; i < (int)sizeof(output); ++i)
		assert(output[i] == 0xa5);
}

static void
expect_auth_failure(const char *authorization)
{
	char decoded[128];

	memset(decoded, 'X', sizeof(decoded));
	assert(httpd_decode_basic_authorization(authorization, decoded,
						 (int)sizeof(decoded)) < 0);
	assert(decoded[0] == '\0');
}

int
main(void)
{
	unsigned char bytes[16];
	char decoded[128];
	int length;

	/* Canonical RFC 4648 examples, including both padding forms. */
	length = b64_decode_strict("YWJj", bytes, sizeof(bytes));
	assert(length == 3 && memcmp(bytes, "abc", 3) == 0);
	length = b64_decode_strict("YQ==", bytes, sizeof(bytes));
	assert(length == 1 && bytes[0] == 'a');
	length = b64_decode_strict("YWI=", bytes, sizeof(bytes));
	assert(length == 2 && memcmp(bytes, "ab", 2) == 0);
	assert(b64_decode_strict("", bytes, sizeof(bytes)) == 0);

	/* Reject non-canonical padding, ignored bytes and truncated tokens. */
	expect_decode_failure("YQ", sizeof(bytes));
	expect_decode_failure("YQ=", sizeof(bytes));
	expect_decode_failure("YQ===", sizeof(bytes));
	expect_decode_failure("YQ=A", sizeof(bytes));
	expect_decode_failure("YR==", sizeof(bytes)); /* non-zero unused pad bits */
	expect_decode_failure("YWJ=", sizeof(bytes)); /* non-zero unused pad bits */
	expect_decode_failure("YQ==\n", sizeof(bytes));
	expect_decode_failure("YQ==!", sizeof(bytes));
	expect_decode_failure("YWJj", 2); /* output capacity */
	expect_decode_failure("AA==", 0);

	/* HTTP scheme matching is case-insensitive; OWS and CRLF are handled. */
	length = httpd_decode_basic_authorization("Basic dXNlcjpwYXNz\r\n",
						  decoded, sizeof(decoded));
	assert(length == 9 && strcmp(decoded, "user:pass") == 0);
	length = httpd_decode_basic_authorization("basic   dXNlcjpwYXNz \t\r\n",
						  decoded, sizeof(decoded));
	assert(length == 9 && strcmp(decoded, "user:pass") == 0);
	length = httpd_decode_basic_authorization("Basic dXNlcjpwYXNz",
						  decoded, sizeof(decoded));
	assert(length == 9 && strcmp(decoded, "user:pass") == 0);

	/* Reject malformed scheme separators, invalid tokens and ambiguous data. */
	expect_auth_failure("Basic");
	expect_auth_failure("Basic\tdXNlcjpwYXNz");
	expect_auth_failure("Basic \tdXNlcjpwYXNz");
	expect_auth_failure("Basic dXNlcjpwYXNz!");
	expect_auth_failure("Basic dXNlcg=="); /* no user/password separator */
	expect_auth_failure("Basic YToAYg=="); /* embedded NUL before suffix */
	expect_auth_failure("Basic  ");
	expect_auth_failure("Bearer dXNlcjpwYXNz");

	/* A decoded credential larger than the destination must fail closed. */
	memset(decoded, 'X', sizeof(decoded));
	assert(httpd_decode_basic_authorization("Basic dXNlcjpwYXNz", decoded, 5) < 0);
	assert(decoded[0] == '\0');

	return 0;
}
