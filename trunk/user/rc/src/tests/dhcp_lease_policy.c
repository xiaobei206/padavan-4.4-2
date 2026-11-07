#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dhcp_lease_policy.h"

int
main(void)
{
	char first[INET_ADDRSTRLEN];
	unsigned int number = 0;

	assert(dhcp_lease_ipv4_is_valid("192.0.2.1"));
	assert(!dhcp_lease_ipv4_is_valid("192.0.2.1;reboot"));
	assert(!dhcp_lease_ipv4_is_valid("192.0.2.1\n"));
	assert(!dhcp_lease_ipv4_is_valid("999.0.2.1"));
	assert(dhcp_lease_netmask_is_valid("255.255.255.0"));
	assert(dhcp_lease_netmask_is_valid("255.255.255.255"));
	assert(!dhcp_lease_netmask_is_valid("255.0.255.0"));
	assert(!dhcp_lease_netmask_is_valid("0.0.0.0"));

	assert(dhcp_lease_ipv4_list_is_valid("192.0.2.1 198.51.100.2", 255, 16, 0));
	assert(dhcp_lease_ipv4_list_is_valid("", 255, 16, 1));
	assert(dhcp_lease_ipv4_list_is_valid(NULL, 255, 16, 1));
	assert(!dhcp_lease_ipv4_list_is_valid("192.0.2.1 198.51.100.2;reboot", 255, 16, 1));
	assert(!dhcp_lease_ipv4_list_is_valid("192.0.2.1\r198.51.100.2", 255, 16, 1));
	assert(!dhcp_lease_ipv4_list_is_valid("192.0.2.1 999.1.1.1", 255, 16, 1));
	assert(!dhcp_lease_ipv4_list_is_valid("192.0.2.1", 3, 16, 1));
	assert(dhcp_lease_ipv4_list_first(" 192.0.2.1 198.51.100.2", first, sizeof(first)) == 1);
	assert(strcmp(first, "192.0.2.1") == 0);
	assert(dhcp_lease_ipv4_list_first("", first, sizeof(first)) == 0);
	assert(dhcp_lease_ipv4_list_first("192.0.2.1,198.51.100.2", first, sizeof(first)) == -1);

	assert(dhcp_lease_decimal_is_valid("1500", 576, 1500, &number) && number == 1500);
	assert(dhcp_lease_decimal_is_valid("0", 0, INT_MAX, &number) && number == 0);
	assert(!dhcp_lease_decimal_is_valid("-1", 0, INT_MAX, &number));
	assert(!dhcp_lease_decimal_is_valid("1501", 576, 1500, &number));
	assert(!dhcp_lease_decimal_is_valid("12x", 0, INT_MAX, &number));
	assert(!dhcp_lease_decimal_is_valid("999999999999999999999", 0, INT_MAX, &number));

	assert(dhcp_lease_route_text_is_safe(""));
	assert(dhcp_lease_route_text_is_safe("192.0.2.0/24 198.51.100.1 0.0.0.0/0 198.51.100.1"));
	assert(!dhcp_lease_route_text_is_safe("192.0.2.0/24;reboot"));
	assert(!dhcp_lease_route_text_is_safe("192.0.2.0/24\nCOMMIT"));
	assert(!dhcp_lease_route_text_is_safe("192.0.2.0/24 198.51.100.1 extra"));

	puts("DHCP lease input policy regression: OK");
	return 0;
}
