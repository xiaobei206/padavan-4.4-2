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
	static const char * const public_callbacks[] = {
		"nvram_get_x",
		"nvram_get_list_x",
		"nvram_get_buf_x",
		"nvram_get_table_x",
		"nvram_match_x",
		"nvram_double_match_x",
		"nvram_match_both_x",
		"nvram_match_list_x",
		"uptime",
		"nvram_dump",
		"firmware_caps_hook",
		"json_system_status",
		"netdev",
		"bandwidth",
		"nvram",
		"nvram_get_ddns",
		"nvram_char_to_ascii",
		"update_variables",
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
		"detect_internet",
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
	int i;

	assert(!httpd_direct_ej_registered(NULL));
	assert(!httpd_direct_ej_registered(""));
	assert(!httpd_direct_ej_allowed(NULL, 0));
	assert(!httpd_direct_ej_allowed(NULL, 1));
	assert(!httpd_direct_ej_allowed("", 0));
	assert(!httpd_direct_ej_allowed("", 1));

	for (i = 0; privileged_callbacks[i] != NULL; ++i) {
		assert(httpd_direct_ej_registered(privileged_callbacks[i]));
		assert(httpd_direct_ej_requires_trusted_session(privileged_callbacks[i]));
		assert(!httpd_direct_ej_allowed(privileged_callbacks[i], 0));
		assert(httpd_direct_ej_allowed(privileged_callbacks[i], 1));
	}

	for (i = 0; public_callbacks[i] != NULL; ++i) {
		assert(httpd_direct_ej_registered(public_callbacks[i]));
		assert(!httpd_direct_ej_requires_trusted_session(public_callbacks[i]));
		assert(httpd_direct_ej_allowed(public_callbacks[i], 0));
		assert(httpd_direct_ej_allowed(public_callbacks[i], 1));
	}

	/* Similar-looking and future callback names fail closed until reviewed. */
	assert(!httpd_direct_ej_registered("wol_action_extra"));
	assert(!httpd_direct_ej_allowed("wol_action_extra", 0));
	assert(!httpd_direct_ej_allowed("wol_action_extra", 1));
	assert(!httpd_direct_ej_registered("unregistered_callback"));
	assert(!httpd_direct_ej_allowed("unregistered_callback", 0));
	assert(!httpd_direct_ej_allowed("unregistered_callback", 1));

	return 0;
}
