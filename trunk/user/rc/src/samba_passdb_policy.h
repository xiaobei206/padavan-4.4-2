#ifndef PADAVAN_SAMBA_PASSDB_POLICY_H
#define PADAVAN_SAMBA_PASSDB_POLICY_H

#include <stddef.h>
#include <string.h>

/* Match the firmware's 127-byte password limit and reject line controls. */
static int
samba_passdb_password_is_safe(const char *password)
{
	const unsigned char *p;
	size_t len;

	if (!password || !*password)
		return 0;

	len = strlen(password);
	if (len >= 128)
		return 0;

	for (p = (const unsigned char *)password; *p; ++p) {
		if (*p < 0x20 || *p == 0x7f)
			return 0;
	}

	return 1;
}

#endif /* PADAVAN_SAMBA_PASSDB_POLICY_H */
