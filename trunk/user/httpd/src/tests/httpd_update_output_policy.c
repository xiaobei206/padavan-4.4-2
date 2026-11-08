#include <assert.h>
#include <stddef.h>

#include "httpd_update_output_policy.h"

int
main(void)
{
	static const char * const public_outputs[] = {
		"netdev",
		"bandwidth",
		NULL
	};
	static const char * const protected_outputs[] = {
		"nvram_get_x",
		"nvram_get_list_x",
		"nvram_get_buf_x",
		"nvram_get_table_x",
		"nvram_dump",
		"nvram",
		"update_variables",
		"asus_nvram_commit",
		"notify_services",
		"wan_action",
		"wol_action",
		"wl_scan_2g",
		"wl_scan_5g",
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
		"scutclient_action",
		"mentohust_action",
		"shadowsocks_action",
		"adbyby_action",
		"update_action",
		"openssl_util_hook",
		"openvpn_srv_cert_hook",
		"openvpn_cli_cert_hook",
		NULL
	};
	int i;

	assert(!httpd_update_output_allowed(NULL, 0));
	assert(!httpd_update_output_allowed(NULL, 1));
	assert(!httpd_update_output_allowed("", 0));
	assert(!httpd_update_output_allowed("", 1));

	for (i = 0; public_outputs[i] != NULL; ++i) {
		assert(httpd_update_output_is_unprivileged(public_outputs[i]));
		assert(httpd_update_output_allowed(public_outputs[i], 0));
		assert(httpd_update_output_allowed(public_outputs[i], 1));
	}

	for (i = 0; protected_outputs[i] != NULL; ++i) {
		assert(!httpd_update_output_is_unprivileged(protected_outputs[i]));
		assert(!httpd_update_output_allowed(protected_outputs[i], 0));
		assert(httpd_update_output_allowed(protected_outputs[i], 1));
	}

	/* Exact names only; selectors that resemble a safe callback stay gated. */
	assert(!httpd_update_output_allowed("netdev_extra", 0));
	assert(!httpd_update_output_allowed("bandwidth2", 0));
	assert(!httpd_update_output_allowed("unregistered_callback", 0));
	assert(httpd_update_output_allowed("unregistered_callback", 1));

	return 0;
}
