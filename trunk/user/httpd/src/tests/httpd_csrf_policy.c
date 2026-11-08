#include <assert.h>
#include <stddef.h>

#include "httpd_csrf_policy.h"

int
main(void)
{
	/* Explicit same-origin metadata: normal hosts, case folding and paths. */
	assert(httpd_csrf_post_allowed("http://router.lan", NULL,
	                               "ROUTER.LAN", 0, 0, 0, 0));
	assert(httpd_csrf_post_allowed("http://router.lan:80", NULL,
	                               "router.lan", 0, 0, 0, 0));
	assert(httpd_csrf_post_allowed("https://router.lan", NULL,
	                               "router.lan:443", 1, 0, 0, 0));
	assert(httpd_csrf_post_allowed(NULL,
	                               "http://router.lan:80/index.asp?x=1",
	                               "router.lan", 0, 0, 0, 0));
	assert(httpd_csrf_post_allowed("http://[fd00::1]:8080", NULL,
	                               "[FD00::1]:8080", 0, 0, 0, 0));

	/* Different host, scheme, or non-default port is cross-origin. */
	assert(!httpd_csrf_post_allowed("http://evil.example", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("https://router.lan", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("http://router.lan:8080", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("null", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("http://user@router.lan", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("http://router.lan/path", NULL,
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed("http://router.lan", NULL,
	                                NULL, 0, 0, 0, 0));

	/* A mismatching Origin cannot be overridden by a same-origin Referer. */
	assert(!httpd_csrf_post_allowed("http://evil.example",
	                                "http://router.lan/index.asp",
	                                "router.lan", 0, 0, 0, 0));
	assert(!httpd_csrf_post_allowed(NULL, "http://evil.example/form",
	                                "router.lan", 0, 0, 0, 0));

	/* Duplicate/malformed header and ambiguous Host states fail closed. */
	assert(!httpd_csrf_post_allowed("http://router.lan", NULL,
	                                "router.lan", 0, 1, 0, 0));
	assert(!httpd_csrf_post_allowed(NULL, "http://router.lan/form",
	                                "router.lan", 0, 0, 1, 0));
	assert(!httpd_csrf_post_allowed("http://router.lan", NULL,
	                                "router.lan", 0, 0, 0, 1));

	/* Preserve compatibility when non-browser clients omit both headers. */
	assert(httpd_csrf_post_allowed(NULL, NULL, NULL, 0, 0, 0, 0));

	/* Header copying strips CRLF/outer whitespace and rejects empty/oversize. */
	{
		char value[32];
		assert(httpd_csrf_copy_header_value(value, sizeof(value),
		                                   "  http://router.lan\r\n"));
		assert(value[0] == 'h');
		assert(!httpd_csrf_copy_header_value(value, sizeof(value), "\r\n"));
		assert(!httpd_csrf_copy_header_value(value, 4, "http://router.lan"));
	}

	return 0;
}
