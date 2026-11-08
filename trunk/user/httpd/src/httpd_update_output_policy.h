/*
 * Authorization policy for update.cgi's request-selected EJ callback.
 *
 * The shipped UI uses only netdev (read-only interface counters) and
 * bandwidth (a bounded traffic-statistics refresh that updates transient
 * rstats state and signals the rstats process). Preserve those two callbacks
 * for authenticated management sessions. Every other selector, including
 * callbacks added in the future, requires the existing trusted-session
 * predicate.
 */
#ifndef HTTPD_UPDATE_OUTPUT_POLICY_H
#define HTTPD_UPDATE_OUTPUT_POLICY_H

#include <string.h>

static int
httpd_update_output_is_unprivileged(const char *pattern)
{
	if (!pattern)
		return 0;

	return (strcmp(pattern, "netdev") == 0 ||
		strcmp(pattern, "bandwidth") == 0);
}

static int
httpd_update_output_allowed(const char *pattern, int trusted_session)
{
	if (!pattern || !*pattern)
		return 0;

	if (httpd_update_output_is_unprivileged(pattern))
		return 1;

	return (trusted_session != 0);
}

#endif /* HTTPD_UPDATE_OUTPUT_POLICY_H */
