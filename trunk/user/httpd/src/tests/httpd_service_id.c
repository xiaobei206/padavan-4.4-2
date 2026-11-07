#include <assert.h>
#include <stddef.h>

#include "../common.h"

struct svcLink svcLinks[] = {
	{ "known_service", NULL },
	{ "another_service", NULL },
	{ NULL, NULL }
};

int
main(void)
{
	assert(LookupServiceId("known_service") == 0);
	assert(LookupServiceId("another_service") == 1);
	assert(LookupServiceId("not-a-service") == -1);
	assert(LookupServiceId("") == -1);
	assert(GetServiceId(2) == NULL);
	assert(GetVariables(2) == NULL);
	return 0;
}
