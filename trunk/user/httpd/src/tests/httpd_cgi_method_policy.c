#include <assert.h>

#include "httpd_cgi_method_policy.h"

enum {
	TEST_HTTP_METHOD_GET = 0,
	TEST_HTTP_METHOD_HEAD,
	TEST_HTTP_METHOD_POST
};

int
main(void)
{
	static const char * const post_only_routes[] = {
		"apply.cgi",
		"applydb.cgi",
		"update.cgi",
		"upgrade.cgi",
		"restore_nv.cgi",
		"restore_st.cgi",
		NULL
	};
	int i;

	for (i = 0; post_only_routes[i] != NULL; ++i) {
		assert(!httpd_cgi_method_allowed(post_only_routes[i],
						 TEST_HTTP_METHOD_GET,
						 TEST_HTTP_METHOD_POST));
		assert(!httpd_cgi_method_allowed(post_only_routes[i],
						 TEST_HTTP_METHOD_HEAD,
						 TEST_HTTP_METHOD_POST));
		assert(httpd_cgi_method_allowed(post_only_routes[i],
						TEST_HTTP_METHOD_POST,
						TEST_HTTP_METHOD_POST));
	}

	/* Match the CGI table's prefix patterns, not only exact route names. */
	assert(!httpd_cgi_method_allowed("apply.cgi.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("update.cgi.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));

	/* applydb.cgi accepts GET-shaped legacy input but mutates persisted
	 * database state and invokes helper scripts, so it must be POST-only.
	 */
	assert(!httpd_cgi_method_allowed("applydb.cgi",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("applydb.cgi",
					 TEST_HTTP_METHOD_HEAD,
					 TEST_HTTP_METHOD_POST));
	assert(httpd_cgi_method_allowed("applydb.cgi",
					TEST_HTTP_METHOD_POST,
					TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("applydb.cgi.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));

	assert(httpd_cgi_method_allowed("system_status_data.asp",
					TEST_HTTP_METHOD_GET,
					TEST_HTTP_METHOD_POST));
	assert(httpd_cgi_method_allowed(NULL,
					TEST_HTTP_METHOD_GET,
					TEST_HTTP_METHOD_POST));
	return 0;
}
