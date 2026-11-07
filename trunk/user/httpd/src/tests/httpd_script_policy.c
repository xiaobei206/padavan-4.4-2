#include <assert.h>

#include "httpd_script_policy.h"

int
main(void)
{
	assert(!httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 0));
	assert(httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 1));

	/* The fixed handler surface remains compatible but explicit. */
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
		int i;

		for (i = 0; fixed_names[i] != NULL; ++i) {
			assert(httpd_script_name_is_fixed(fixed_names[i]));
			assert(httpd_script_dispatch_allowed(fixed_names[i], 0));
		}
	}
	assert(!httpd_script_name_is_fixed(NULL));
	assert(!httpd_script_name_is_fixed("unknown_script.sh"));
	assert(!httpd_script_dispatch_allowed("unknown_script.sh", 0));
	assert(httpd_script_dispatch_allowed("unknown_script.sh", 1));
	assert(!httpd_script_dispatch_allowed(NULL, 0));

	/* Non-executable configuration and credential files keep their
	 * separate existing authorization policies. */
	assert(httpd_script_file_write_allowed(0, 0));
	assert(httpd_script_file_write_allowed(2, 0));
	assert(httpd_script_file_write_allowed(3, 0));
	return 0;
}
