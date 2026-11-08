#include <assert.h>
#include <stddef.h>

#include "httpd_direct_ej_policy.h"

int
main(void)
{
	static const char * const trusted_callbacks[] = {
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
	static const char * const self_guarded_callbacks[] = {
		"nvram_dump",
		"update_variables",
		NULL
	};
	static const char * const bounded_refresh_callbacks[] = {
		"bandwidth",
		"nvram_get_ddns",
		"detect_internet",
		NULL
	};
	static const char * const read_only_callbacks[] = {
		"nvram_get_x",
		"nvram_get_list_x",
		"nvram_get_buf_x",
		"nvram_get_table_x",
		"nvram_match_x",
		"nvram_double_match_x",
		"nvram_match_both_x",
		"nvram_match_list_x",
		"uptime",
		"firmware_caps_hook",
		"json_system_status",
		"netdev",
		"nvram",
		"nvram_char_to_ascii",
		"login_state_hook",
		"wanlink",
		"lanlink",
		"nf_values",
		"get_parameter",
		"get_nvram_list",
		"get_flash_time",
		"get_static_client",
		"get_static_ccount",
		"get_vpns_client",
		"wl_auth_list",
		"wl_scan_5g",
		"wl_bssid_5g",
		"wl_bssid_2g",
		"wl_scan_2g",
		"shown_language_option",
		"hardware_pins",
		"dump_syslog",
		"dump_eth_mib",
		"get_usb_ports_info",
		"get_ext_ports_info",
		"disk_pool_mapping_info",
		"available_disk_names_and_sizes",
		"get_usb_share_list",
		"get_AiDisk_status",
		"get_all_accounts",
		"get_permissions_of_account",
		"get_folder_tree",
		"get_share_tree",
		"scutclient_status",
		"scutclient_version",
		"mentohust_status",
		"shadowsocks_status",
		"rules_count",
		"pdnsd_status",
		"dns2tcp_status",
		"dnsproxy_status",
		"zerotier_status",
		"ddnsto_status",
		"aliyundrive_status",
		"sqm_status",
		"smartdns_status",
		"frpc_status",
		"frps_status",
		"adbyby_status",
		"dnsforwarder_status",
		"openssl_util_hook",
		"openvpn_srv_cert_hook",
		"openvpn_cli_cert_hook",
		NULL
	};
	const char * const *name;
	int i;

	assert(!httpd_direct_ej_allowed(NULL, 0));
	assert(!httpd_direct_ej_allowed(NULL, 1));
	assert(!httpd_direct_ej_allowed("", 0));
	assert(!httpd_direct_ej_allowed("", 1));
	assert(httpd_direct_ej_classify(NULL) == HTTPD_DIRECT_EJ_UNKNOWN);
	assert(httpd_direct_ej_classify("") == HTTPD_DIRECT_EJ_UNKNOWN);

	for (i = 0; trusted_callbacks[i] != NULL; ++i) {
		assert(httpd_direct_ej_classify(trusted_callbacks[i]) ==
		       HTTPD_DIRECT_EJ_TRUSTED_MUTATION);
		assert(httpd_direct_ej_requires_trusted_session(trusted_callbacks[i]));
		assert(!httpd_direct_ej_allowed(trusted_callbacks[i], 0));
		assert(httpd_direct_ej_allowed(trusted_callbacks[i], 1));
	}

	for (name = self_guarded_callbacks; *name != NULL; ++name) {
		assert(httpd_direct_ej_classify(*name) == HTTPD_DIRECT_EJ_SELF_GUARDED);
		assert(!httpd_direct_ej_requires_trusted_session(*name));
		assert(httpd_direct_ej_allowed(*name, 0));
		assert(httpd_direct_ej_allowed(*name, 1));
	}

	for (name = bounded_refresh_callbacks; *name != NULL; ++name) {
		assert(httpd_direct_ej_classify(*name) == HTTPD_DIRECT_EJ_BOUNDED_REFRESH);
		assert(!httpd_direct_ej_requires_trusted_session(*name));
		assert(httpd_direct_ej_allowed(*name, 0));
		assert(httpd_direct_ej_allowed(*name, 1));
	}

	for (name = read_only_callbacks; *name != NULL; ++name) {
		assert(httpd_direct_ej_classify(*name) == HTTPD_DIRECT_EJ_READ_ONLY);
		assert(!httpd_direct_ej_requires_trusted_session(*name));
		assert(httpd_direct_ej_allowed(*name, 0));
		assert(httpd_direct_ej_allowed(*name, 1));
	}

	/* Unknown, look-alike and future registrations fail closed. */
	assert(httpd_direct_ej_classify("wol_action_extra") == HTTPD_DIRECT_EJ_UNKNOWN);
	assert(httpd_direct_ej_classify("unregistered_callback") == HTTPD_DIRECT_EJ_UNKNOWN);
	assert(httpd_direct_ej_requires_trusted_session("unregistered_callback"));
	assert(!httpd_direct_ej_allowed("wol_action_extra", 0));
	assert(!httpd_direct_ej_allowed("wol_action_extra", 1));
	assert(!httpd_direct_ej_allowed("unregistered_callback", 0));
	assert(!httpd_direct_ej_allowed("unregistered_callback", 1));

	return 0;
}
