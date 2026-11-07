#include <assert.h>

#include "httpd_access_policy.h"

int
main(void)
{
	/* No authenticated login must never disclose a private key. */
	assert(!httpd_private_key_access_allowed(0, 0));
	assert(!httpd_private_key_access_allowed(0, 1));

	/* An authenticated HTTPS peer from an untrusted network is still denied. */
	assert(!httpd_private_key_access_allowed(1, 0));

	/* An authenticated LAN/VPN peer retains the intended access. */
	assert(httpd_private_key_access_allowed(1, 1));
	return 0;
}
