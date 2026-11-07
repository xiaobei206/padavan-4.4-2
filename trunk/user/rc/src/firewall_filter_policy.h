#ifndef PADAVAN_FIREWALL_FILTER_POLICY_H
#define PADAVAN_FIREWALL_FILTER_POLICY_H

/* URL patterns are embedded in a quoted iptables-restore rule. */
static int
firewall_url_filter_value_is_safe(const char *value)
{
	const unsigned char *p = (const unsigned char *)value;

	if (!p)
		return 0;

	for (; *p; ++p) {
		if (*p < 0x20 || *p == 0x7f || *p == '"' || *p == '\\')
			return 0;
	}

	return 1;
}

/* ICMP type-list entries are numeric values in the range 0..255. */
static int
firewall_icmp_type_is_safe(const char *value)
{
	const unsigned char *p = (const unsigned char *)value;
	unsigned int type = 0;

	if (!p || !*p)
		return 0;

	for (; *p; ++p) {
		unsigned int digit;

		if (*p < '0' || *p > '9')
			return 0;

		digit = (unsigned int)(*p - '0');
		if (type > (255U - digit) / 10U)
			return 0;

		type = type * 10U + digit;
	}

	return 1;
}

#endif /* PADAVAN_FIREWALL_FILTER_POLICY_H */
