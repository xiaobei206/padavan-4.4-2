#ifndef PADAVAN_DHCP_LEASE_POLICY_H
#define PADAVAN_DHCP_LEASE_POLICY_H

#include <arpa/inet.h>
#include <stddef.h>
#include <string.h>

/* Check canonical dotted-quad syntax, including the all-zero route prefix. */
static int
dhcp_lease_ipv4_text_is_valid(const char *value)
{
	struct in_addr address;

	return value && inet_pton(AF_INET, value, &address) == 1;
}

/* Match the existing RC is_valid_ipv4() contract: 0.0.0.0 is not a host. */
static int
dhcp_lease_ipv4_is_valid(const char *value)
{
	return dhcp_lease_ipv4_text_is_valid(value) &&
	       strcmp(value, "0.0.0.0") != 0;
}

/* RFC classless routes permit 0.0.0.0 only for the /0 default route. */
static int
dhcp_lease_route_destination_is_valid(const char *address,
                                      unsigned int prefix_length)
{
	if (prefix_length > 32U)
		return 0;
	if (prefix_length == 0U)
		return address && strcmp(address, "0.0.0.0") == 0;

	return dhcp_lease_ipv4_is_valid(address);
}

/* A netmask must be non-zero and contain one contiguous run of leading ones. */
static int
dhcp_lease_netmask_is_valid(const char *value)
{
	struct in_addr address;
	unsigned int mask, inverse;

	if (!value || inet_pton(AF_INET, value, &address) != 1)
		return 0;

	mask = ntohl(address.s_addr);
	if (mask == 0)
		return 0;

	inverse = ~mask;
	return (inverse & (inverse + 1U)) == 0;
}

/* Validate a whitespace-separated list of IPv4 addresses with explicit bounds. */
static int
dhcp_lease_ipv4_list_is_valid(const char *value, size_t max_length,
                              unsigned int max_items, int allow_empty)
{
	const unsigned char *p;
	unsigned int items = 0;
	size_t length;

	if (!value)
		return allow_empty ? 1 : 0;

	length = strlen(value);
	if (length > max_length)
		return 0;

	p = (const unsigned char *)value;
	while (*p) {
		char address[INET_ADDRSTRLEN];
		const unsigned char *start;
		size_t token_length;

		while (*p == ' ' || *p == '\t')
			++p;
		if (!*p)
			break;

		start = p;
		while (*p && *p != ' ' && *p != '\t')
			++p;
		token_length = (size_t)(p - start);
		if (token_length == 0 || token_length >= sizeof(address))
			return 0;
		if (++items > max_items)
			return 0;

		memcpy(address, start, token_length);
		address[token_length] = '\0';
		if (!dhcp_lease_ipv4_is_valid(address))
			return 0;
	}

	return items > 0 || allow_empty;
}

/* Return 1 for a first address, 0 for an empty list and -1 for invalid input. */
static int
dhcp_lease_ipv4_list_first(const char *value, char *out, size_t out_size)
{
	const unsigned char *p;
	const unsigned char *start;
	size_t token_length;

	if (!out || out_size == 0)
		return -1;
	out[0] = '\0';

	if (!dhcp_lease_ipv4_list_is_valid(value, 255, 16, 1))
		return -1;
	if (!value)
		return 0;

	p = (const unsigned char *)value;
	while (*p == ' ' || *p == '\t')
		++p;
	if (!*p)
		return 0;

	start = p;
	while (*p && *p != ' ' && *p != '\t')
		++p;
	token_length = (size_t)(p - start);
	if (token_length >= out_size)
		return -1;

	memcpy(out, start, token_length);
	out[token_length] = '\0';
	return 1;
}

/* Strict decimal parser: no signs, whitespace, suffixes or overflow. */
static int
dhcp_lease_decimal_is_valid(const char *value, unsigned int minimum,
                            unsigned int maximum, unsigned int *result)
{
	const unsigned char *p = (const unsigned char *)value;
	unsigned int number = 0;

	if (!p || !*p || minimum > maximum)
		return 0;

	for (; *p; ++p) {
		unsigned int digit;

		if (*p < '0' || *p > '9')
			return 0;
		digit = (unsigned int)(*p - '0');
		if (number > maximum / 10U ||
		    (number == maximum / 10U && digit > maximum % 10U))
			return 0;
		number = number * 10U + digit;
	}

	if (number < minimum)
		return 0;
	if (result)
		*result = number;
	return 1;
}

/* Route options are parsed as data, never as shell commands. Bound their size
 * and alphabet before storing them for the route-table parser. */
static int
dhcp_lease_route_text_is_safe(const char *value)
{
	const unsigned char *p = (const unsigned char *)value;
	size_t length;

	if (!p)
		return 0;
	length = strlen(value);
	if (length > 2048)
		return 0;

	for (; *p; ++p) {
		if (!((*p >= '0' && *p <= '9') || *p == '.' ||
		      *p == '/' || *p == ' ' || *p == '\t'))
			return 0;
	}

	return 1;
}

#endif /* PADAVAN_DHCP_LEASE_POLICY_H */
