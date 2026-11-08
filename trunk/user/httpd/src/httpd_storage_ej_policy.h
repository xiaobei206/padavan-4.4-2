/*
 * Authorization policy for state-changing storage callbacks invoked from
 * server-side ASP templates. update.cgi has its own request-selector gate;
 * this policy protects the independent direct-template dispatch path too.
 */
#ifndef HTTPD_STORAGE_EJ_POLICY_H
#define HTTPD_STORAGE_EJ_POLICY_H

#include <stddef.h>
#include <string.h>

static int
httpd_storage_ej_requires_trusted_session(const char *name)
{
	static const char * const mutating_callbacks[] = {
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
	int i;

	if (!name || !*name)
		return 0;

	for (i = 0; mutating_callbacks[i] != NULL; ++i) {
		if (strcmp(name, mutating_callbacks[i]) == 0)
			return 1;
	}

	return 0;
}

static int
httpd_storage_ej_allowed(const char *name, int trusted_session)
{
	if (!name || !*name)
		return 0;

	if (trusted_session)
		return 1;

	return !httpd_storage_ej_requires_trusted_session(name);
}

#endif /* HTTPD_STORAGE_EJ_POLICY_H */
