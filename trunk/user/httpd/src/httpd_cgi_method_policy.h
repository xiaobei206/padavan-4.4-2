/*
 * HTTP method policy for HTTPD endpoints that perform privileged state
 * changes or consume uploaded artifacts.
 *
 * MIME matching treats protected route names as prefixes ("*.cgi*" and
 * "*.asp*"). Keep the method and trusted-session gates before input/template
 * dispatch so query parameters cannot trigger a mutation through GET or HEAD.
 */
#ifndef HTTPD_CGI_METHOD_POLICY_H
#define HTTPD_CGI_METHOD_POLICY_H

#include <string.h>

static int
httpd_storage_route_requires_trusted_session(const char *path)
{
	static const char * const trusted_storage_routes[] = {
		"aidisk/initial_account.asp",
		"aidisk/create_account.asp",
		"aidisk/delete_account.asp",
		"aidisk/modify_account.asp",
		"aidisk/set_account_permission.asp",
		"aidisk/create_sharedfolder.asp",
		"aidisk/delete_sharedfolder.asp",
		"aidisk/modify_sharedfolder.asp",
		"aidisk/switch_share_mode.asp",
		"aidisk/switch_AiDisk_app.asp",
		NULL
	};
	size_t i;

	if (!path || !*path)
		return 0;

	for (i = 0; trusted_storage_routes[i] != NULL; ++i) {
		size_t route_len = strlen(trusted_storage_routes[i]);

		if (strncmp(path, trusted_storage_routes[i], route_len) == 0)
			return 1;
	}

	return 0;
}

static int
httpd_cgi_method_allowed(const char *path, int method_id, int post_method_id)
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
	size_t i;

	if (!path || method_id == post_method_id)
		return 1;

	for (i = 0; post_only_routes[i] != NULL; ++i) {
		size_t route_len = strlen(post_only_routes[i]);

		if (strncmp(path, post_only_routes[i], route_len) == 0)
			return 0;
	}

	return !httpd_storage_route_requires_trusted_session(path);
}

#endif /* HTTPD_CGI_METHOD_POLICY_H */
