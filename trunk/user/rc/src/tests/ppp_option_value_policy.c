#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ppp_option_value_policy.h"

int
main(void)
{
	char out[64];
	char exact[5];
	char small[20];
	char long_value[301];
	char expanded[11];

	assert(ppp_option_value_escape("user'\\name", out, sizeof(out)) == 0);
	assert(strcmp(out, "user\\'\\\\name") == 0);

	/* Exactly enough space includes the NUL byte. */
	assert(ppp_option_value_escape("a'b", exact, sizeof(exact)) == 0);
	assert(strcmp(exact, "a\\'b") == 0);

	/* A character that needs escaping must not trigger silent truncation. */
	memset(expanded, '\\', sizeof(expanded) - 1);
	expanded[sizeof(expanded) - 1] = '\0';
	small[0] = 'x';
	assert(ppp_option_value_escape(expanded, small, sizeof(small)) == -1);
	assert(errno == EOVERFLOW);
	assert(small[0] == '\0');

	memset(long_value, 'x', sizeof(long_value) - 1);
	long_value[sizeof(long_value) - 1] = '\0';
	assert(ppp_option_value_escape(long_value, out, sizeof(out)) == -1);
	assert(errno == EOVERFLOW);
	assert(out[0] == '\0');

	assert(ppp_option_value_escape("name\npassword attacker", out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(out[0] == '\0');
	assert(ppp_option_value_escape("name\rvalue", out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(ppp_option_value_escape(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);

	/* The explicitly multiline user-options field preserves LF/TAB only. */
	assert(ppp_option_text_escape("debug\nlogfile /tmp/a\tfoo", out, sizeof(out)) == 0);
	assert(strcmp(out, "debug\nlogfile /tmp/a\tfoo") == 0);
	assert(ppp_option_text_escape("option 'quoted'\\value", out, sizeof(out)) == 0);
	assert(strcmp(out, "option \\'quoted\\'\\\\value") == 0);
	assert(ppp_option_text_escape("bad\rline", out, sizeof(out)) == -1);
	assert(errno == EINVAL);

	assert(ppp_config_line_value_copy("1234", exact, sizeof(exact)) == 0);
	assert(strcmp(exact, "1234") == 0);
	assert(ppp_config_line_value_copy("12345", exact, sizeof(exact)) == -1);
	assert(errno == EOVERFLOW);
	assert(exact[0] == '\0');

	assert(ppp_config_line_value_copy("peer.example", out, sizeof(out)) == 0);
	assert(strcmp(out, "peer.example") == 0);
	assert(ppp_config_line_value_copy("user name", out, sizeof(out)) == 0);
	assert(strcmp(out, "user name") == 0);
	assert(ppp_config_line_value_copy("peer\nsection attack", out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(out[0] == '\0');
	assert(ppp_config_line_value_copy("peer\rsection attack", out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(ppp_config_line_value_copy("bad\tvalue", out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(ppp_config_line_value_copy(long_value, out, sizeof(out)) == -1);
	assert(errno == EOVERFLOW);
	assert(out[0] == '\\0');
	assert(ppp_config_line_value_copy(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);

	puts("PPP option value escaping regression: OK");
	return 0;
}
