/*
 * Authorization policy for privileged callbacks invoked while rendering
 * direct ASP templates. update.cgi has its own request-selector policy, but
 * call_asp() is a separate dispatch path and must not inherit that omission.
 */
#ifndef HTTPD_DIRECT_EJ_POLICY_H
#define HTTPD_DIRECT_EJ_POLICY_H

#include <stddef.h>
#include <string.h>

#include "httpd_storage_ej_policy.h"

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
	if (!name || !*name)
		return 0;

	if (trusted_session)
		return 1;

	return !httpd_direct_ej_requires_trusted_session(name);
}

#endif /* HTTPD_DIRECT_EJ_POLICY_H */
