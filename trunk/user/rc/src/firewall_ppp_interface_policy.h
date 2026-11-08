#ifndef PADAVAN_FIREWALL_PPP_INTERFACE_POLICY_H
#define PADAVAN_FIREWALL_PPP_INTERFACE_POLICY_H

#include <string.h>

/* VPN lease and per-client firewall rules consume pppd interface names.
 * Accept only exact ppp<decimal-unit> tokens in the configured server range. */
static int
firewall_ppp_interface_is_safe(const char *value, unsigned int min_unit)
{
	const unsigned char *p;
	size_t len;
	unsigned int unit = 0;

	if (!value)
		return 0;

	len = strlen(value);
	if (len < 4 || len >= 16 || strncmp(value, "ppp", 3) != 0)
		return 0;

	p = (const unsigned char *)value + 3;
	if (p[0] == '0' && p[1] != '\0')
		return 0;
	if (!*p)
		return 0;

	for (; *p; ++p) {
		unsigned int digit;

		if (*p < '0' || *p > '9')
			return 0;

		digit = (unsigned int)(*p - '0');
		if (unit > (65535U - digit) / 10U)
			return 0;

		unit = unit * 10U + digit;
	}

	return (unit >= min_unit);
}

#endif /* PADAVAN_FIREWALL_PPP_INTERFACE_POLICY_H */
