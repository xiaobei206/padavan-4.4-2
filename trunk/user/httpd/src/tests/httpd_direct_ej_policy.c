#include <assert.h>
#include <stddef.h>

#include "httpd_direct_ej_policy.h"

int
main(void)
{
	static const char * const privileged_callbacks[] = {
		"asus_nvram_commit",
		"notify_services",
		"wan_action",
		"wol_action",
		"scutclient_action",
		"mentohust_action",
		"shadowsocks_action",
		"adbyby_action",
		"update_action",
		"set_AiDisk_status",
		"safely_remove_disk",
		"set_account_permission",
		"initial_account",
		"create_account",
		"delete_account",
		"modify_account",
		"create_sharedfolder",
		"delete_sharedfolder",
		"modify_sharedfolder",
		"set_share_mode",
		NULL
	};
	static const char * const read_only_callbacks[] = {
		"login_state_hook",
		"wanlink",
		"lanlink",
		"netdev",
		"bandwidth",
		"openssl_util_hook",
		"openvpn_srv_cert_hook",
		"openvpn_cli_cert_hook",
		"get_AiDisk_status",
		"get_all_accounts",
		"get_permissions_of_account",
		"get_folder_tree",
		"get_share_tree",
		"json_system_status",
		NULL
	};
	int i;

	assert(!httpd_direct_ej_allowed(NULL, 0));
	assert(!httpd_direct_ej_allowed(NULL, 1));
	assert(!httpd_direct_ej_allowed("", 0));
	assert(!httpd_direct_ej_allowed("", 1));

	for (i = 0; privileged_callbacks[i] != NULL; ++i) {
		assert(httpd_direct_ej_requires_trusted_session(privileged_callbacks[i]));
		assert(!httpd_direct_ej_allowed(privileged_callbacks[i], 0));
		assert(httpd_direct_ej_allowed(privileged_callbacks[i], 1));
	}

	for (i = 0; read_only_callbacks[i] != NULL; ++i) {
		assert(!httpd_direct_ej_requires_trusted_session(read_only_callbacks[i]));
		assert(httpd_direct_ej_allowed(read_only_callbacks[i], 0));
		assert(httpd_direct_ej_allowed(read_only_callbacks[i], 1));
	}

	/* Similar names do not inherit the action restriction; unknown handlers
	 * retain the legacy direct-template behavior pending explicit review.
	 */
	assert(!httpd_direct_ej_requires_trusted_session("wol_action_extra"));
	assert(httpd_direct_ej_allowed("wol_action_extra", 0));
	assert(!httpd_direct_ej_requires_trusted_session("unregistered_callback"));
	assert(httpd_direct_ej_allowed("unregistered_callback", 0));
	assert(httpd_direct_ej_allowed("unregistered_callback", 1));

	return 0;
}
