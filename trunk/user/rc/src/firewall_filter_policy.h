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

#endif /* PADAVAN_FIREWALL_FILTER_POLICY_H */
