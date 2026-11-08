#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "httpd_js_output_policy.h"

static void
assert_escaped(const unsigned char *input, const char *expected)
{
	char output[512];
	size_t input_len = strlen((const char *)input);
	size_t input_offset = 0;
	size_t output_len = 0;

	while (input_offset < input_len) {
		char escaped[6];
		size_t consumed = 0;
		size_t length = httpd_js_escape_next(input + input_offset,
						     escaped, &consumed);

		assert(consumed > 0);
		assert(consumed <= input_len - input_offset);
		assert(length > 0);
		assert(length <= sizeof(escaped));
		assert(output_len + length < sizeof(output));
		memcpy(output + output_len, escaped, length);
		input_offset += consumed;
		output_len += length;
	}

	output[output_len] = '\0';
	assert(strcmp(output, expected) == 0);
}

int
main(void)
{
	static const unsigned char attack[] = "'\\</script>";
	static const unsigned char controls[] = {
		'\b', '\f', '\n', '\r', '\t', 0x01, 0x7f, 0
	};
	static const unsigned char unicode_separators[] = {
		0xe2, 0x80, 0xa8, 0xe2, 0x80, 0xa9, 0
	};
	static const unsigned char utf8_text[] = {
		0xc3, 0xa9, ' ', 0xe4, 0xb8, 0xad, 0
	};
	char output[6];
	size_t consumed = 99;

	assert_escaped((const unsigned char *)"plain text", "plain text");
	assert_escaped(attack, "\\'\\\\\\x3c/script>");
	assert_escaped(controls, "\\b\\f\\n\\r\\t\\x01\\x7f");
	assert_escaped(unicode_separators, "\\u2028\\u2029");
	assert_escaped(utf8_text, "\xc3\xa9 \xe4\xb8\xad");

	assert(httpd_js_escape_next(NULL, output, &consumed) == 0);
	assert(consumed == 0);
	consumed = 99;
	assert(httpd_js_escape_next((const unsigned char *)"", output,
				    &consumed) == 0);
	assert(consumed == 0);

	return 0;
}
