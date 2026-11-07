#ifndef PADAVAN_FIREWALL_FILTER_POLICY_H
#define PADAVAN_FIREWALL_FILTER_POLICY_H

#include <arpa/inet.h>
#include <string.h>

/* IPv4 values are emitted as unquoted tokens in iptables-restore input. */
static int
firewall_ipv4_address_is_safe(const char *value)
{
	struct in_addr address;

	return value && inet_pton(AF_INET, value, &address) == 1;
}

/* Interface names become bare -i/-o tokens in iptables-restore files.
 * Linux IFNAMSIZ is 16 bytes including NUL; permit ordinary interface
 * characters and the conventional trailing '+' wildcard used by iptables. */
static int
firewall_interface_name_is_safe(const char *value)
{
	const unsigned char *p;
	size_t len;

	if (!value)
		return 0;

	len = strlen(value);
	if (len == 0 || len > 15)
		return 0;

	p = (const unsigned char *)value;
	if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
	      (*p >= '0' && *p <= '9')))
		return 0;

	for (; *p; ++p) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' ||
		    *p == ':' || *p == '-')
			continue;
		if (*p == '+' && p[1] == '\0')
			return 1;
		return 0;
	}

	return 1;
}

/* VPN lease interface names must be exact ppp<decimal-unit> tokens. */
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


/* Decimal protocol numbers from NVRAM must stay within the IP protocol field. */
static int
firewall_decimal_value_is_safe(const char *value, unsigned int minimum,
                               unsigned int maximum)
{
	const unsigned char *p = (const unsigned char *)value;
	unsigned int number = 0;

	if (!p || !*p)
		return 0;

	for (; *p; ++p) {
		unsigned int digit;

		if (*p < '0' || *p > '9')
			return 0;

		digit = (unsigned int)(*p - '0');
		if (number > (maximum - digit) / 10U)
			return 0;
		number = number * 10U + digit;
	}

	return number >= minimum;
}

static int
firewall_ip_protocol_number_is_safe(const char *value)
{
	return firewall_decimal_value_is_safe(value, 0U, 255U);
}

static int
firewall_port_number_is_safe(const char *value)
{
	return firewall_decimal_value_is_safe(value, 1U, 65535U);
}

static int
firewall_port_span_is_safe(const char *start, const char *end,
                           unsigned int *number)
{
	const unsigned char *p = (const unsigned char *)start;
	const unsigned char *limit = (const unsigned char *)end;
	unsigned int value = 0;

	if (!start || !end || start == end)
		return 0;

	for (; p < limit; ++p) {
		unsigned int digit;

		if (*p < '0' || *p > '9')
			return 0;
		digit = (unsigned int)(*p - '0');
		if (value > (65535U - digit) / 10U)
			return 0;
		value = value * 10U + digit;
	}

	if (value == 0)
		return 0;
	*number = value;
	return 1;
}

/* Firewall port fields accept one port or one inclusive low:high range. */
static int
firewall_port_range_is_safe(const char *value, int allow_empty)
{
	const char *separator;
	unsigned int first, last;

	if (!value)
		return 0;
	if (!*value)
		return allow_empty ? 1 : 0;

	separator = strchr(value, ':');
	if (!separator)
		return firewall_port_number_is_safe(value);
	if (strchr(separator + 1, ':'))
		return 0;
	if (!firewall_port_span_is_safe(value, separator, &first) ||
	    !firewall_port_span_is_safe(separator + 1, value + strlen(value), &last))
		return 0;

	return first <= last;
}

/* TCP flag suffixes are selected from this fixed set by the shipped UI. */
static int
firewall_tcp_flags_are_safe(const char *value)
{
	if (!value)
		return 0;
	return (!*value || !strcmp(value, "ALL") || !strcmp(value, "SYN") ||
	        !strcmp(value, "ACK") || !strcmp(value, "FIN") ||
	        !strcmp(value, "RST") || !strcmp(value, "URG") ||
	        !strcmp(value, "PSH"));
}

static int
firewall_schedule_date_is_safe(const char *value)
{
	int i;

	if (!value || strlen(value) != 7)
		return 0;
	for (i = 0; i < 7; ++i) {
		if (value[i] != '0' && value[i] != '1')
			return 0;
	}
	return 1;
}

/* HHMMHHMM is consumed byte-for-byte by iptables' time match options. */
static int
firewall_schedule_time_is_safe(const char *value)
{
	unsigned int start_hour, start_minute, end_hour, end_minute;
	int i;

	if (!value || strlen(value) != 8)
		return 0;
	for (i = 0; i < 8; ++i) {
		if (value[i] < '0' || value[i] > '9')
			return 0;
	}

	start_hour = (unsigned int)(value[0] - '0') * 10U +
	             (unsigned int)(value[1] - '0');
	start_minute = (unsigned int)(value[2] - '0') * 10U +
	               (unsigned int)(value[3] - '0');
	end_hour = (unsigned int)(value[4] - '0') * 10U +
	           (unsigned int)(value[5] - '0');
	end_minute = (unsigned int)(value[6] - '0') * 10U +
	             (unsigned int)(value[7] - '0');

	return start_hour <= 23U && end_hour <= 23U &&
	       start_minute <= 59U && end_minute <= 59U;
}

/*
 * Invalid or explicitly disabled schedules are inactive, not unqualified
 * (all-time) rules. A valid all-days/all-day schedule remains active.
 */
static int
firewall_schedule_is_active(const char *date, const char *time)
{
	return firewall_schedule_date_is_safe(date) &&
	       firewall_schedule_time_is_safe(time) &&
	       strcmp(date, "0000000") != 0;
}

#endif /* PADAVAN_FIREWALL_FILTER_POLICY_H */
