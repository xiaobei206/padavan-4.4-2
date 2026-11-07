#include <stddef.h>
#include <assert.h>
#include "../firewall_filter_policy.h"

int
main(void)
{
	assert(firewall_ppp_interface_is_safe("ppp10", 10));
	assert(firewall_ppp_interface_is_safe("ppp59", 10));
	assert(firewall_ppp_interface_is_safe("ppp65535", 10));
	assert(!firewall_ppp_interface_is_safe(NULL, 10));
	assert(!firewall_ppp_interface_is_safe("ppp", 10));
	assert(!firewall_ppp_interface_is_safe("ppp9", 10));
	assert(!firewall_ppp_interface_is_safe("ppp10+", 10));
	assert(!firewall_ppp_interface_is_safe("ppp10x", 10));
	assert(!firewall_ppp_interface_is_safe("ppp010", 10));
	assert(!firewall_ppp_interface_is_safe("ppp00000000000010", 10));
	assert(!firewall_ppp_interface_is_safe("ppp10\n-A INPUT -j ACCEPT", 10));
	assert(!firewall_ppp_interface_is_safe("ppp65536", 10));
	assert(!firewall_ppp_interface_is_safe("ppp999999999999", 10));

	assert(firewall_ipv4_address_is_safe("192.168.1.1"));
	assert(firewall_ipv4_address_is_safe("0.0.0.0"));
	assert(!firewall_ipv4_address_is_safe(NULL));
	assert(!firewall_ipv4_address_is_safe(""));
	assert(!firewall_ipv4_address_is_safe("192.168.1.1\n-A INPUT -j ACCEPT"));
	assert(!firewall_ipv4_address_is_safe("192.168.1.1 -j ACCEPT"));
	assert(!firewall_ipv4_address_is_safe("192.168.1.999"));
	assert(!firewall_ipv4_address_is_safe("192.168.1"));
	assert(!firewall_ipv4_address_is_safe("192.168.1.1\rCOMMIT"));

	/* Interface strings are written as raw iptables-restore tokens. */
	assert(firewall_interface_name_is_safe("br0"));
	assert(firewall_interface_name_is_safe("eth2.2"));
	assert(firewall_interface_name_is_safe("ppp0"));
	assert(firewall_interface_name_is_safe("apcli0"));
	assert(firewall_interface_name_is_safe("wwan0"));
	assert(firewall_interface_name_is_safe("eth0:1"));
	assert(firewall_interface_name_is_safe("ppp10+"));
	assert(!firewall_interface_name_is_safe(NULL));
	assert(!firewall_interface_name_is_safe(""));
	assert(!firewall_interface_name_is_safe("-eth0"));
	assert(!firewall_interface_name_is_safe("+eth0"));
	assert(!firewall_interface_name_is_safe("eth0++"));
	assert(!firewall_interface_name_is_safe("eth0/wan"));
	assert(!firewall_interface_name_is_safe("eth0;COMMIT"));
	assert(!firewall_interface_name_is_safe("eth0 -j ACCEPT"));
	assert(!firewall_interface_name_is_safe("eth0\n-A INPUT -j ACCEPT"));
	assert(!firewall_interface_name_is_safe("1234567890123456"));

	assert(firewall_url_filter_value_is_safe("example.com"));
	assert(firewall_url_filter_value_is_safe("www.example.com/path?q=1&x=two"));
	assert(firewall_url_filter_value_is_safe("video-site.example/channel-1"));
	assert(firewall_url_filter_value_is_safe(""));
	assert(!firewall_url_filter_value_is_safe("evil\"\n-A INPUT -j ACCEPT\n#"));
	assert(!firewall_url_filter_value_is_safe("evil\\\n-A INPUT -j ACCEPT"));
	assert(!firewall_url_filter_value_is_safe("line\nfeed"));
	assert(!firewall_url_filter_value_is_safe("line\rreturn"));
	assert(!firewall_url_filter_value_is_safe("tab\tvalue"));
	assert(!firewall_url_filter_value_is_safe("delete\177char"));
	assert(!firewall_url_filter_value_is_safe(NULL));

	assert(firewall_icmp_type_is_safe("0"));
	assert(firewall_icmp_type_is_safe("8"));
	assert(firewall_icmp_type_is_safe("255"));
	assert(firewall_icmp_type_is_safe("0008"));
	assert(!firewall_icmp_type_is_safe(NULL));
	assert(!firewall_icmp_type_is_safe(""));
	assert(!firewall_icmp_type_is_safe("256"));
	assert(!firewall_icmp_type_is_safe("-1"));
	assert(!firewall_icmp_type_is_safe("8/1"));
	assert(!firewall_icmp_type_is_safe("8\nCOMMIT"));
	assert(!firewall_icmp_type_is_safe("8\r-A INPUT -j ACCEPT"));
	assert(!firewall_icmp_type_is_safe("8\t"));
	assert(!firewall_icmp_type_is_safe("8 -A INPUT -j ACCEPT"));

	assert(firewall_ip_protocol_number_is_safe("0"));
	assert(firewall_ip_protocol_number_is_safe("6"));
	assert(firewall_ip_protocol_number_is_safe("255"));
	assert(firewall_ip_protocol_number_is_safe("0006"));
	assert(!firewall_ip_protocol_number_is_safe(NULL));
	assert(!firewall_ip_protocol_number_is_safe(""));
	assert(!firewall_ip_protocol_number_is_safe("256"));
	assert(!firewall_ip_protocol_number_is_safe("-1"));
	assert(!firewall_ip_protocol_number_is_safe("6\nCOMMIT"));

	assert(firewall_port_number_is_safe("1"));
	assert(firewall_port_number_is_safe("65535"));
	assert(!firewall_port_number_is_safe(NULL));
	assert(!firewall_port_number_is_safe(""));
	assert(!firewall_port_number_is_safe("0"));
	assert(!firewall_port_number_is_safe("65536"));
	assert(!firewall_port_number_is_safe("80\nCOMMIT"));

	assert(firewall_port_range_is_safe("80", 0));
	assert(firewall_port_range_is_safe("1:65535", 0));
	assert(firewall_port_range_is_safe("", 1));
	assert(!firewall_port_range_is_safe("", 0));
	assert(!firewall_port_range_is_safe(NULL, 1));
	assert(!firewall_port_range_is_safe("80:81:82", 0));
	assert(!firewall_port_range_is_safe("81:80", 0));
	assert(!firewall_port_range_is_safe("80:65536", 0));
	assert(!firewall_port_range_is_safe("80\n-A INPUT -j ACCEPT", 0));
	assert(!firewall_port_range_is_safe("80,81", 0));

	assert(firewall_tcp_flags_are_safe(""));
	assert(firewall_tcp_flags_are_safe("ALL"));
	assert(firewall_tcp_flags_are_safe("SYN"));
	assert(firewall_tcp_flags_are_safe("PSH"));
	assert(!firewall_tcp_flags_are_safe(NULL));
	assert(!firewall_tcp_flags_are_safe("SYN,ACK"));
	assert(!firewall_tcp_flags_are_safe("SYN\n-A INPUT -j ACCEPT"));

	assert(firewall_schedule_date_is_safe("1111111"));
	assert(firewall_schedule_date_is_safe("0101010"));
	assert(firewall_schedule_date_is_safe("0000000"));
	assert(!firewall_schedule_date_is_safe(NULL));
	assert(!firewall_schedule_date_is_safe("111111"));
	assert(!firewall_schedule_date_is_safe("111111x"));
	assert(!firewall_schedule_date_is_safe("111111\n"));

	assert(firewall_schedule_time_is_safe("00002359"));
	assert(firewall_schedule_time_is_safe("23592359"));
	assert(firewall_schedule_time_is_safe("08001730"));
	assert(!firewall_schedule_time_is_safe(NULL));
	assert(!firewall_schedule_time_is_safe("0800173"));
	assert(!firewall_schedule_time_is_safe("0800173x"));
	assert(!firewall_schedule_time_is_safe("24001730"));
	assert(!firewall_schedule_time_is_safe("08601730"));
	assert(!firewall_schedule_time_is_safe("08002400"));
	assert(!firewall_schedule_time_is_safe("08001760"));
	assert(!firewall_schedule_time_is_safe("0800\n-A INPUT"));

	assert(firewall_schedule_is_active("1111111", "00002359"));
	assert(firewall_schedule_is_active("0101010", "08001730"));
	assert(!firewall_schedule_is_active("0000000", "00002359"));
	assert(!firewall_schedule_is_active("0000000", "08001730"));
	assert(!firewall_schedule_is_active("111111x", "00002359"));
	assert(!firewall_schedule_is_active("1111111", "0800173x"));
	assert(!firewall_schedule_is_active(NULL, "00002359"));
	assert(!firewall_schedule_is_active("1111111", NULL));

	return 0;
}
