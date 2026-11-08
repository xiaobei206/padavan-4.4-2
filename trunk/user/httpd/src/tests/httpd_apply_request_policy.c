#include <assert.h>
#include <stddef.h>

#include "httpd_apply_request_policy.h"

int
main(void)
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

	assert(!httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 0));
	assert(httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 1));

	for (i = 0; trusted_modes[i] != NULL; ++i) {
		assert(httpd_apply_action_mode_requires_trusted_session(trusted_modes[i]));
		assert(!httpd_apply_request_allowed(trusted_modes[i], NULL, 0));
		assert(httpd_apply_request_allowed(trusted_modes[i], NULL, 1));
	}

	/* Direct eval and generic /tmp script paths are privileged, even though
	 * neither is represented by one of sys_script()'s fixed handlers. */
	assert(!httpd_apply_request_allowed(" Apply ", "networkmap_refresh", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "unknown_script.sh", 0));
	assert(httpd_apply_request_allowed(" Apply ", "networkmap_refresh", 1));
	assert(httpd_apply_request_allowed(" Apply ", "unknown_script.sh", 1));

	/* Existing harmless fixed handlers remain available, the six known
	 * fixed side-effect handlers are restricted, and syscmd.sh retains its
	 * special stale-command-clearing path. */
	assert(httpd_apply_request_allowed(" Apply ", "syslog.sh", 0));
	assert(httpd_apply_request_allowed(" Apply ", "syscmd.sh", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "lpr_remove", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "mfp_requeue", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "mfp_monopolize", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "ddnsclient", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "hostname_check", 0));
	assert(!httpd_apply_request_allowed(" Apply ", "dlna_rescan", 0));

	/* Exact matching prevents prefix or malformed action names from
	 * inheriting a privileged operation's semantics. */
	assert(!httpd_apply_action_mode_requires_trusted_session(NULL));
	assert(!httpd_apply_action_mode_requires_trusted_session(""));
	assert(!httpd_apply_action_mode_requires_trusted_session(" Reboot"));
	assert(!httpd_apply_action_mode_requires_trusted_session(" Reboot ."));
	assert(httpd_apply_request_allowed("unknown action", NULL, 0));
	assert(httpd_apply_request_allowed(NULL, NULL, 0));
	assert(httpd_apply_request_allowed(" Apply ", "", 0));

	return 0;
}
