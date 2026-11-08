#include <assert.h>

#include "httpd_script_policy.h"

int
main(void)
{
	static const char * const fixed_names[] = {
		"syscmd.sh",
		"syslog.sh",
		"wan.sh",
		"printer.sh",
		"lpr_remove",
		"mfp_requeue",
		"mfp_monopolize",
		"wlan11a.sh",
		"wlan11b.sh",
		"eth_wan.sh",
		"eth_lan1.sh",
		"eth_lan2.sh",
		"eth_lan3.sh",
		"eth_lan4.sh",
		"leases.sh",
		"vpns_list.sh",
		"iptable.sh",
		"route.sh",
		"conntrack.sh",
		"ddnsclient",
		"hostname_check",
		"dlna_rescan",
		NULL
	};
	static const char * const trusted_only_names[] = {
		"lpr_remove",
		"mfp_requeue",
		"mfp_monopolize",
		"ddnsclient",
		"hostname_check",
		"dlna_rescan",
		NULL
	};
	int i, j, requires_trusted;

	assert(!httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 0));
	assert(httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 1));

	/* The complete fixed dispatch surface remains explicit. Side-effecting
	 * handlers require a trusted session; no-op/read handlers remain
	 * compatible for authenticated off-LAN management. */
	for (i = 0; fixed_names[i] != NULL; ++i) {
		requires_trusted = 0;
		for (j = 0; trusted_only_names[j] != NULL; ++j) {
			if (strcmp(fixed_names[i], trusted_only_names[j]) == 0) {
				requires_trusted = 1;
				break;
			}
		}

		assert(httpd_script_name_is_fixed(fixed_names[i]));
		assert(httpd_script_name_requires_trusted_session(fixed_names[i]) ==
		       requires_trusted);
		assert(httpd_script_dispatch_allowed(fixed_names[i], 1));
		assert(httpd_script_dispatch_allowed(fixed_names[i], 0) ==
		       !requires_trusted);
	}

	/* syscmd.sh must still reach its handler to clear a stale global buffer;
	 * execution of the queued command has its own get_login_safe() check. */
	assert(httpd_script_dispatch_allowed("syscmd.sh", 0));
	assert(!httpd_script_name_requires_trusted_session("syscmd.sh"));

	assert(!httpd_script_name_is_fixed(NULL));
	assert(!httpd_script_name_requires_trusted_session(NULL));
	assert(!httpd_script_name_requires_trusted_session(""));
	assert(!httpd_script_name_is_fixed("unknown_script.sh"));
	assert(!httpd_script_dispatch_allowed("unknown_script.sh", 0));
	assert(httpd_script_dispatch_allowed("unknown_script.sh", 1));
	assert(!httpd_script_dispatch_allowed(NULL, 0));
	assert(!httpd_script_dispatch_allowed("", 1));

	/* Non-executable configuration and credential files keep their
	 * separate existing authorization policies. */
	assert(httpd_script_file_write_allowed(0, 0));
	assert(httpd_script_file_write_allowed(2, 0));
	assert(httpd_script_file_write_allowed(3, 0));
	return 0;
}
