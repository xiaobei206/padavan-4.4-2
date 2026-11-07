#ifndef PADAVAN_SAMBA_CONFIG_VALUE_POLICY_H
#define PADAVAN_SAMBA_CONFIG_VALUE_POLICY_H

/*
 * Values copied from NVRAM into smb.conf must never create another option
 * line or invoke Samba's backslash/percent substitutions.
 */
static int
samba_config_value_is_safe(const char *value)
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

#endif /* PADAVAN_SAMBA_CONFIG_VALUE_POLICY_H */
