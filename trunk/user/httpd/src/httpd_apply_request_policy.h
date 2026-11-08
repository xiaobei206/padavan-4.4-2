/*
 * Per-operation trust policy for authenticated CGI management requests.
 *
 * This is an additional authorization boundary for high-impact apply.cgi
 * actions and action_script dispatch. It deliberately does not implement
 * request-token, Origin/Referer, or general CSRF validation.
 */
#ifndef HTTPD_APPLY_REQUEST_POLICY_H
#define HTTPD_APPLY_REQUEST_POLICY_H

#include <string.h>

#include "httpd_script_policy.h"

static int
httpd_apply_action_mode_requires_trusted_session(const char *action_mode)
{
	static const char * const trusted_modes[] = {
		" SystemCmd ",
		" ClearLog ",
		" ClearssrplusLog ",
		" Reboot ",
		" Shutdown ",
		" FreeMemory ",
		" RestoreNVRAM ",
		" RestoreStorage ",
		" CommitFlash ",
		" ExportConfOVPNC ",
		" CreateCertOVPNS ",
		" CreateCertHTTPS ",
		" NTPSyncNow ",
		NULL
	};
	int i;

	if (!action_mode || !*action_mode)
		return 0;

	for (i = 0; trusted_modes[i] != NULL; ++i) {
		if (strcmp(action_mode, trusted_modes[i]) == 0)
			return 1;
	}

	return 0;
}

/*
 * Use the same explicit fixed-script policy as sys_script(). This also
 * rejects generic /tmp script dispatch before update_variables_ex() can
 * apply unrelated request changes. networkmap_refresh is a direct eval()
 * special case rather than a sys_script() name, so keep it explicitly
 * privileged even if it is later added to the fixed-handler list.
 */
static int
httpd_apply_request_allowed(const char *action_mode, const char *action_script,
			    int trusted_session)
{
	if (trusted_session)
		return 1;

	if (httpd_apply_action_mode_requires_trusted_session(action_mode))
		return 0;

	if (action_script && *action_script) {
		if (strcmp(action_script, "networkmap_refresh") == 0)
			return 0;
		if (!httpd_script_dispatch_allowed(action_script, 0))
			return 0;
	}

	return 1;
}

#endif /* HTTPD_APPLY_REQUEST_POLICY_H */
