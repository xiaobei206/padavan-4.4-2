/*
 * HTTP method policy for HTTPD CGI endpoints that perform privileged
 * state changes or consume uploaded artifacts.
 *
 * MIME matching treats these route names as prefixes ("*.cgi*"); mirror that
 * behavior here so a suffixed path cannot reach the same handler via GET.
 */
#ifndef HTTPD_CGI_METHOD_POLICY_H
#define HTTPD_CGI_METHOD_POLICY_H

#include <string.h>

static int
httpd_cgi_method_allowed(const char *path, int method_id, int post_method_id)
{
	static const char * const post_only_routes[] = {
		"apply.cgi",
		"applydb.cgi",
		"upgrade.cgi",
		"restore_nv.cgi",
		"restore_st.cgi",
		NULL
	};
	size_t i;

	if (!path || method_id == post_method_id)
		return 1;

	for (i = 0; post_only_routes[i] != NULL; ++i) {
		size_t route_len = strlen(post_only_routes[i]);

		if (strncmp(path, post_only_routes[i], route_len) == 0)
			return 0;
	}

	return 1;
}

#endif /* HTTPD_CGI_METHOD_POLICY_H */
