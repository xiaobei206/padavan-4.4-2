#include <assert.h>
#include <limits.h>

#include "httpd_login_policy.h"

int
main(void)
{
	/* The existing policy expires only after, not at, the timeout boundary. */
	assert(!httpd_login_session_expired(100UL, 100UL, 30UL));
	assert(!httpd_login_session_expired(129UL, 100UL, 30UL));
	assert(!httpd_login_session_expired(130UL, 100UL, 30UL));
	assert(httpd_login_session_expired(131UL, 100UL, 30UL));

	/* Unsigned subtraction must survive uptime counter wraparound. */
	assert(!httpd_login_session_expired(2UL, ULONG_MAX - 2UL, 5UL));
	assert(httpd_login_session_expired(2UL, ULONG_MAX - 2UL, 4UL));

	return 0;
}
