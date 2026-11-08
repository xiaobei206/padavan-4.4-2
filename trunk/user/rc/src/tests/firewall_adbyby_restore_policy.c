#include <stdio.h>
#include "firewall_adbyby_restore_policy.h"

int
main(void)
{
	if (firewall_adbyby_restore_is_allowed(0, 0) ||
	    firewall_adbyby_restore_is_allowed(0, 1) ||
	    firewall_adbyby_restore_is_allowed(1, 0) ||
	    !firewall_adbyby_restore_is_allowed(1, 1)) {
		fprintf(stderr, "Adbyby restore preflight policy regression failed\n");
		return 1;
	}
	puts("Adbyby restore preflight policy regression: OK");
	return 0;
}
