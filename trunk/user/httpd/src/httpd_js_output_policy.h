/*
 * Escape an untrusted byte stream for insertion into a single-quoted
 * JavaScript string literal. The caller supplies the surrounding quotes.
 *
 * In addition to JavaScript metacharacters, escape '<' so untrusted input
 * cannot terminate an enclosing HTML script element, and escape U+2028 /
 * U+2029 because older JavaScript engines treat them as line terminators.
 */
#ifndef HTTPD_JS_OUTPUT_POLICY_H
#define HTTPD_JS_OUTPUT_POLICY_H

#include <stddef.h>

static size_t
httpd_js_escape_next(const unsigned char *input, char output[6],
		     size_t *input_consumed)
{
	unsigned char c;

	if (!input || !*input || !output || !input_consumed) {
		if (input_consumed)
			*input_consumed = 0;
		return 0;
	}

	c = input[0];
	*input_consumed = 1;

	if (c == 0xe2 && input[1] == 0x80 &&
	    (input[2] == 0xa8 || input[2] == 0xa9)) {
		output[0] = '\\';
		output[1] = 'u';
		output[2] = '2';
		output[3] = '0';
		output[4] = '2';
		output[5] = (input[2] == 0xa8) ? '8' : '9';
		*input_consumed = 3;
		return 6;
	}

	switch (c) {
	case '\'':
		output[0] = '\\';
		output[1] = '\'';
		return 2;
	case '\\':
		output[0] = '\\';
		output[1] = '\\';
		return 2;
	case '<':
		output[0] = '\\';
		output[1] = 'x';
		output[2] = '3';
		output[3] = 'c';
		return 4;
	case '\b':
		output[0] = '\\';
		output[1] = 'b';
		return 2;
	case '\f':
		output[0] = '\\';
		output[1] = 'f';
		return 2;
	case '\n':
		output[0] = '\\';
		output[1] = 'n';
		return 2;
	case '\r':
		output[0] = '\\';
		output[1] = 'r';
		return 2;
	case '\t':
		output[0] = '\\';
		output[1] = 't';
		return 2;
	default:
		if (c < 0x20 || c == 0x7f) {
			static const char hex[] = "0123456789abcdef";
			output[0] = '\\';
			output[1] = 'x';
			output[2] = hex[(c >> 4) & 0x0f];
			output[3] = hex[c & 0x0f];
			return 4;
		}
		output[0] = (char)c;
		return 1;
	}
}

#endif /* HTTPD_JS_OUTPUT_POLICY_H */
