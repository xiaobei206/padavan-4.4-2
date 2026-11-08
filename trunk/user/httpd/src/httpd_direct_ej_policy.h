/*
 * Explicit authorization classification for callbacks dispatched while
 * rendering direct ASP templates. update.cgi has its own selector policy,
 * but call_asp() is an independent dispatch path.
 *
 * Every web_ex.c::ej_handlers registration must appear exactly once here.
 * Unknown callbacks fail closed. New registrations must be assigned a
 * privilege class rather than inheriting public access by omission.
 *
 * SELF_GUARDED callbacks enforce their own request/file policy. BOUNDED_REFRESH
 * callbacks perform only fixed-path status refresh work (no user command
 * execution). TRUSTED_MUTATION callbacks require the existing trusted-session
 * predicate. Storage mutators are additionally checked by storage EJ policy.
 */
#ifndef HTTPD_DIRECT_EJ_POLICY_H
#define HTTPD_DIRECT_EJ_POLICY_H

#include <stddef.h>
#include <string.h>

#include "httpd_storage_ej_policy.h"

enum {
	HTTPD_DIRECT_EJ_UNKNOWN = 0,
	HTTPD_DIRECT_EJ_READ_ONLY,
	HTTPD_DIRECT_EJ_BOUNDED_REFRESH,
	HTTPD_DIRECT_EJ_SELF_GUARDED,
	HTTPD_DIRECT_EJ_TRUSTED_MUTATION
};

static int
httpd_direct_ej_classify(const char *name)
{
	static const struct {
		const char *name;
		int classification;
	} callback_policies[] = {
		{ "nvram_get_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_get_list_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_get_buf_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_get_table_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_match_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_double_match_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_match_both_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_match_list_x", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "uptime", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_dump", HTTPD_DIRECT_EJ_SELF_GUARDED },
		{ "firmware_caps_hook", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "json_system_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "netdev", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "bandwidth", HTTPD_DIRECT_EJ_BOUNDED_REFRESH },
		{ "nvram", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "nvram_get_ddns", HTTPD_DIRECT_EJ_BOUNDED_REFRESH },
		{ "nvram_char_to_ascii", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "update_variables", HTTPD_DIRECT_EJ_SELF_GUARDED },
		{ "asus_nvram_commit", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "notify_services", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "login_state_hook", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wanlink", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "lanlink", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wan_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "wol_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "nf_values", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_parameter", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_nvram_list", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_flash_time", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_static_client", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_static_ccount", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_vpns_client", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wl_auth_list", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wl_scan_5g", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wl_bssid_5g", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wl_bssid_2g", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "wl_scan_2g", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "shown_language_option", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "hardware_pins", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "detect_internet", HTTPD_DIRECT_EJ_BOUNDED_REFRESH },
		{ "dump_syslog", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "dump_eth_mib", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_usb_ports_info", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_ext_ports_info", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "disk_pool_mapping_info", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "available_disk_names_and_sizes", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_usb_share_list", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_AiDisk_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "set_AiDisk_status", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "get_all_accounts", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "safely_remove_disk", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "get_permissions_of_account", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "set_account_permission", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "get_folder_tree", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "get_share_tree", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "initial_account", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "create_account", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "delete_account", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "modify_account", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "create_sharedfolder", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "delete_sharedfolder", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "modify_sharedfolder", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "set_share_mode", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "scutclient_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "scutclient_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "scutclient_version", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "mentohust_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "mentohust_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "shadowsocks_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "shadowsocks_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "rules_count", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "pdnsd_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "dns2tcp_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "dnsproxy_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "zerotier_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "ddnsto_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "aliyundrive_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "sqm_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "smartdns_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "frpc_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "frps_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "adbyby_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "adbyby_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "dnsforwarder_status", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "update_action", HTTPD_DIRECT_EJ_TRUSTED_MUTATION },
		{ "openssl_util_hook", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "openvpn_srv_cert_hook", HTTPD_DIRECT_EJ_READ_ONLY },
		{ "openvpn_cli_cert_hook", HTTPD_DIRECT_EJ_READ_ONLY },
		{ NULL, HTTPD_DIRECT_EJ_UNKNOWN }
	};
	size_t i;

	if (!name || !*name)
		return HTTPD_DIRECT_EJ_UNKNOWN;

	for (i = 0; callback_policies[i].name != NULL; ++i) {
		if (strcmp(name, callback_policies[i].name) == 0)
			return callback_policies[i].classification;
	}

	return HTTPD_DIRECT_EJ_UNKNOWN;
}

static int
httpd_direct_ej_requires_trusted_session(const char *name)
{
	int classification;

	if (!name || !*name)
		return 1;

	classification = httpd_direct_ej_classify(name);
	if (classification == HTTPD_DIRECT_EJ_UNKNOWN)
		return 1;

	return classification == HTTPD_DIRECT_EJ_TRUSTED_MUTATION ||
	       !httpd_storage_ej_allowed(name, 0);
}

static int
httpd_direct_ej_allowed(const char *name, int trusted_session)
{
	int classification;

	if (!name || !*name)
		return 0;

	classification = httpd_direct_ej_classify(name);
	if (classification == HTTPD_DIRECT_EJ_UNKNOWN)
		return 0;

	if (classification == HTTPD_DIRECT_EJ_TRUSTED_MUTATION && !trusted_session)
		return 0;

	return httpd_storage_ej_allowed(name, trusted_session);
}

#endif /* HTTPD_DIRECT_EJ_POLICY_H */
