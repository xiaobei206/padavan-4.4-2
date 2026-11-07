#ifndef PADAVAN_ROUTER_ACCOUNT_POLICY_H
#define PADAVAN_ROUTER_ACCOUNT_POLICY_H

#include <stddef.h>
#include <string.h>

/*
 * Account names become Unix passwd/group fields, Samba principals and
 * storage metadata filenames. Keep this policy byte-oriented and independent
 * of the process locale so control characters and delimiters cannot cross
 * those formats.
 */
static int
router_account_name_is_valid(const char *name)
{
	const unsigned char *p;
	size_t length = 0;

	if (!name || !*name)
		return 0;

	for (p = (const unsigned char *)name; *p; ++p) {
		if (++length > 31)
			return 0;
		if (!((*p >= 'a' && *p <= 'z') ||
		      (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') ||
		      *p == '_' || *p == '-' || *p == '.'))
			return 0;
	}

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return 0;

	return 1;
}

/* These names are created by the firmware or identify the root account. */
static int
router_account_name_is_reserved(const char *name, const char *management_name)
{
	if (!name || !*name)
		return 0;

	if (!strcmp(name, "root") || !strcmp(name, "nobody") ||
	    !strcmp(name, "sshd"))
		return 1;

	if (management_name && *management_name &&
	    !strcmp(name, management_name))
		return 1;

	return 0;
}

#endif /* PADAVAN_ROUTER_ACCOUNT_POLICY_H */
