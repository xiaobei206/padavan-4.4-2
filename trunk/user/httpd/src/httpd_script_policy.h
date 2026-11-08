/*
 * Server-side policy for HTTPD-managed executable script files.
 *
 * A script uploaded through the management interface is stored executable
 * and may later run with root privileges. It must only be writable from a
 * trusted management session.
 */
#ifndef HTTPD_SCRIPT_POLICY_H
#define HTTPD_SCRIPT_POLICY_H

#define HTTPD_FILE_TYPE_SCRIPT 1

#include <string.h>

/*
 * Fixed handlers are a deliberately small compatibility surface. They do
 * not accept arbitrary command text; generic /tmp execution remains a
 * separate trusted-session-only path. Keep this list in sync with sys_script.
 */
static int
httpd_script_name_is_fixed(const char *name)
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

	if (!name || !*name)
		return 0;

	for (i = 0; fixed_names[i] != NULL; ++i) {
		if (strcmp(name, fixed_names[i]) == 0)
			return 1;
	}

	return 0;
}

/*
 * These fixed handlers are not read-only: they signal daemons, alter
 * temporary NVRAM state, or enqueue privileged RC work. A valid login from
 * an untrusted network is not sufficient to invoke them. The remaining
 * fixed handlers preserve their historical compatibility behavior.
 */
static int
httpd_script_name_requires_trusted_session(const char *name)
{
	static const char * const trusted_names[] = {
		"lpr_remove",
		"mfp_requeue",
		"mfp_monopolize",
		"ddnsclient",
		"hostname_check",
		"dlna_rescan",
		NULL
	};
	int i;

	if (!name || !*name)
		return 0;

	for (i = 0; trusted_names[i] != NULL; ++i) {
		if (strcmp(name, trusted_names[i]) == 0)
			return 1;
	}

	return 0;
}

static int
httpd_script_dispatch_allowed(const char *name, int trusted_session)
{
	if (!name || !*name)
		return 0;

	if (trusted_session)
		return 1;

	/* syscmd.sh remains dispatchable so its handler can clear a stale
	 * process-global command buffer; actual execution is separately gated. */
	return httpd_script_name_is_fixed(name) &&
	       !httpd_script_name_requires_trusted_session(name);
}

static int
httpd_script_file_write_allowed(int file_type, int trusted_session)
{
	return (file_type != HTTPD_FILE_TYPE_SCRIPT || trusted_session);
}

#endif
