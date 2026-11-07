#ifndef PADAVAN_SAMBA_SHARE_POLICY_H
#define PADAVAN_SAMBA_SHARE_POLICY_H

#include <stddef.h>
#include <string.h>
#include "../../shared/src/include/router_account_policy.h"
/*
 * This firmware maps http_username to UID 0 in /etc/passwd. Anonymous SMB
 * sessions therefore must use the existing locked, non-root nobody account.
 */
#define SAMBA_GUEST_ACCOUNT "nobody"

/*
 * Validate values emitted into Samba/vsftpd configuration files.
 * Filesystem names can originate on removable media; a newline can turn a
 * name into another smb.conf directive, while backslashes and percent signs
 * have parser-specific escaping/substitution behavior.
 */
static int
samba_conf_text_is_safe(const char *value)
{
	const unsigned char *p;

	if (!value)
		return 0;

	for (p = (const unsigned char *)value; *p; ++p) {
		if (*p < 0x20 || *p == 0x7f || *p == '\\' || *p == '%')
			return 0;
	}

	return 1;
}

/* A share label is also a path component supplied to generated config. */
static int
samba_share_name_is_safe(const char *name)
{
	const unsigned char *p;
	size_t length = 0;

	if (!name || !*name || !samba_conf_text_is_safe(name))
		return 0;
	if (name[0] == '.' && (name[1] == '\0' ||
	    (name[1] == '.' && name[2] == '\0')))
		return 0;

	for (p = (const unsigned char *)name; *p; ++p) {
		if (*p == '/' || *p == '[' || *p == ']' ||
		    *p == '#' || *p == ';')
			return 0;
		if (++length > 255)
			return 0;
	}

	return 1;
}

/* Storage shares must remain below the firmware's removable-media root. */
static int
samba_mount_path_is_safe(const char *path)
{
	const char *component, *p;

	if (!path || strncmp(path, "/media/", 7) != 0 ||
	    !path[7] || !samba_conf_text_is_safe(path))
		return 0;

	component = path + 7;
	for (p = component; ; ++p) {
		if (*p == '#' || *p == ';')
			return 0;
		if (*p == '/' || *p == '\0') {
			size_t length = (size_t)(p - component);

			if (length == 0 ||
			    (length == 1 && component[0] == '.') ||
			    (length == 2 && component[0] == '.' && component[1] == '.'))
				return 0;
			if (*p == '\0')
				break;
			component = p + 1;
		}
	}

	return 1;
}

/* User names are emitted as comma-separated Samba ACL tokens and argv. */
static int
samba_acl_user_is_safe(const char *name)
{
	if (!router_account_name_is_valid(name) ||
	    router_account_name_is_reserved(name, NULL) ||
	    !strcmp(name, "anonymous"))
		return 0;

	/* Samba list tokens must not begin with an option-like character. */
	return ((*name >= 'a' && *name <= 'z') ||
		(*name >= 'A' && *name <= 'Z') ||
		(*name >= '0' && *name <= '9') || *name == '_');
}

#endif /* PADAVAN_SAMBA_SHARE_POLICY_H */
