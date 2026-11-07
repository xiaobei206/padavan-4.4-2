#include <assert.h>
#include <stdio.h>
#include "router_account_policy.h"

int main(void)
{
	assert(router_account_name_is_valid("admin"));
	assert(router_account_name_is_valid("user_2"));
	assert(router_account_name_is_valid("_svc-1.2"));
	assert(router_account_name_is_valid("a"));
	assert(!router_account_name_is_valid(NULL));
	assert(!router_account_name_is_valid(""));
	assert(!router_account_name_is_valid("."));
	assert(!router_account_name_is_valid(".."));
	assert(!router_account_name_is_valid("bad:name"));
	assert(!router_account_name_is_valid("bad,name"));
	assert(!router_account_name_is_valid("bad name"));
	assert(!router_account_name_is_valid("bad\\name"));
	assert(!router_account_name_is_valid("bad\nroot:x:0:0::/:/bin/sh"));
	assert(!router_account_name_is_valid("bad\tname"));
	assert(!router_account_name_is_valid("bad%name"));
	assert(!router_account_name_is_valid("用户"));

	{
		char too_long[33];
		int i;
		for (i = 0; i < 32; ++i)
			too_long[i] = 'a';
		too_long[32] = '\0';
		assert(!router_account_name_is_valid(too_long));
	}

	assert(router_account_name_is_reserved("root", "admin"));
	assert(router_account_name_is_reserved("nobody", "admin"));
	assert(router_account_name_is_reserved("sshd", "admin"));
	assert(router_account_name_is_reserved("admin", "admin"));
	assert(!router_account_name_is_reserved("storage", "admin"));
	assert(!router_account_name_is_reserved("storage", NULL));

	puts("Router account-name and reserved-identity policy: PASS");
	return 0;
}
