#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "samba_passdb_policy.h"

int
main(void)
{
	char max_password[128];
	char overlong_password[129];
	char utf8_password[] = "router-密码-2026";

	memset(max_password, 'x', sizeof(max_password) - 1);
	max_password[sizeof(max_password) - 1] = '\0';
	memset(overlong_password, 'y', sizeof(overlong_password) - 1);
	overlong_password[sizeof(overlong_password) - 1] = '\0';

	assert(!samba_passdb_password_is_safe(NULL));
	assert(!samba_passdb_password_is_safe(""));
	assert(samba_passdb_password_is_safe("normal-password"));
	assert(samba_passdb_password_is_safe(utf8_password));
	assert(samba_passdb_password_is_safe(max_password));
	assert(!samba_passdb_password_is_safe(overlong_password));
	assert(!samba_passdb_password_is_safe("line\nbreak"));
	assert(!samba_passdb_password_is_safe("line\rbreak"));
	assert(!samba_passdb_password_is_safe("tab\tbreak"));
	assert(!samba_passdb_password_is_safe("control\001byte"));
	assert(!samba_passdb_password_is_safe("delete\177byte"));

	puts("Samba passdb password policy: PASS");
	return 0;
}
