#include <stddef.h>
#include <assert.h>
#include "../firewall_filter_policy.h"

int
main(void)
{
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
	return 0;
}
