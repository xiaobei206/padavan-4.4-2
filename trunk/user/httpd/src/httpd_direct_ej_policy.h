/*
 * Authorization policy for callbacks invoked while rendering direct ASP
 * templates. update.cgi has its own selector policy, but call_asp() is an
 * independent dispatch path.
 *
 * Keep registered_callbacks[] in exact parity with web_ex.c::ej_handlers.
 * New callbacks are denied until explicitly classified here; this prevents
 * a new EJ registration from silently inheriting legacy public access.
 */
#ifndef HTTPD_DIRECT_EJ_POLICY_H
#define HTTPD_DIRECT_EJ_POLICY_H

#include <stddef.h>
#include <string.h>

#include "httpd_storage_ej_policy.h"

static int
httpd_direct_ej_registered(const char *name)
{
	static const char * const registered_callbacks[] = {
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
		"asus_nvram_commit",
		"notify_services",
		"login_state_hook",
		"wanlink",
		"lanlink",
		"wan_action",
		"wol_action",
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
		"set_AiDisk_status",
		"get_all_accounts",
		"safely_remove_disk",
		"get_permissions_of_account",
		"set_account_permission",
		"get_folder_tree",
		"get_share_tree",
		"initial_account",
		"create_account",
		"delete_account",
		"modify_account",
		"create_sharedfolder",
		"delete_sharedfolder",
		"modify_sharedfolder",
		"set_share_mode",
		"scutclient_action",
		"scutclient_status",
		"scutclient_version",
		"mentohust_action",
		"mentohust_status",
		"shadowsocks_action",
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
		"adbyby_action",
		"adbyby_status",
		"dnsforwarder_status",
		"update_action",
		"openssl_util_hook",
		"openvpn_srv_cert_hook",
		"openvpn_cli_cert_hook",
		NULL
	};
	size_t i;

	if (!name || !*name)
		return 0;

	for (i = 0; registered_callbacks[i] != NULL; ++i) {
		if (strcmp(name, registered_callbacks[i]) == 0)
			return 1;
	}

	return 0;
}

static int
httpd_direct_ej_requires_trusted_session(const char *name)
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
		NULL
	};
	size_t i;

	if (!name || !*name)
		return 0;

	if (!httpd_storage_ej_allowed(name, 0))
		return 1;

	for (i = 0; privileged_callbacks[i] != NULL; ++i) {
		if (strcmp(name, privileged_callbacks[i]) == 0)
			return 1;
	}

	return 0;
}

static int
httpd_direct_ej_allowed(const char *name, int trusted_session)
{
	/* An unclassified or newly registered callback must never default-open. */
	if (!httpd_direct_ej_registered(name))
		return 0;

	if (trusted_session)
		return 1;

	return !httpd_direct_ej_requires_trusted_session(name);
}

#endif /* HTTPD_DIRECT_EJ_POLICY_H */
