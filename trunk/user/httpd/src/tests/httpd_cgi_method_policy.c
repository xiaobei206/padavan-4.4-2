#include <assert.h>
#include <stddef.h>

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
		"wol_action.asp",
		"Shadowsocks_action.asp",
		"mentohust_action.asp",
		"scutclient_action.asp",
		"Ad_action.asp",
		"bit_action.asp",
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

	/* Match the CGI/ASP route table's prefix behavior, not only exact names. */
	assert(!httpd_cgi_method_allowed("apply.cgi.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("update.cgi.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("aidisk/create_account.asp.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("aidisk/switch_AiDisk_app.asp.extra",
					 TEST_HTTP_METHOD_HEAD,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("wol_action.asp.extra",
					 TEST_HTTP_METHOD_GET,
					 TEST_HTTP_METHOD_POST));
	assert(!httpd_cgi_method_allowed("Shadowsocks_action.asp.extra",
					 TEST_HTTP_METHOD_HEAD,
					 TEST_HTTP_METHOD_POST));

	{
		static const char * const storage_routes[] = {
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
		for (i = 0; storage_routes[i] != NULL; ++i)
			assert(httpd_storage_route_requires_trusted_session(storage_routes[i]));
	}
	assert(!httpd_storage_route_requires_trusted_session("wol_action.asp"));
	assert(!httpd_storage_route_requires_trusted_session("bit_action.asp"));

	assert(httpd_storage_route_requires_trusted_session("aidisk/create_account.asp.extra"));
	assert(!httpd_storage_route_requires_trusted_session("aidisk/get_all_accounts.asp"));
	assert(!httpd_storage_route_requires_trusted_session("system_status_data.asp"));
	assert(!httpd_storage_route_requires_trusted_session(NULL));

	/* applydb.cgi accepts GET-shaped legacy input but mutates persistent state. */
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
