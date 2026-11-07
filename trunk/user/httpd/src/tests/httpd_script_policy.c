#include <assert.h>

#include "httpd_script_policy.h"

int
main(void)
{
	assert(!httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 0));
	assert(httpd_script_file_write_allowed(HTTPD_FILE_TYPE_SCRIPT, 1));

	/* Non-executable configuration and credential files keep their
	 * separate existing authorization policies. */
	assert(httpd_script_file_write_allowed(0, 0));
	assert(httpd_script_file_write_allowed(2, 0));
	assert(httpd_script_file_write_allowed(3, 0));
	return 0;
}
