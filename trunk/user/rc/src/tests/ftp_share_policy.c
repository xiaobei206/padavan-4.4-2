#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "../ftp_share_policy.h"

int
main(void)
{
	assert(ftp_share_mode_normalize(1) == 1);
	assert(ftp_share_mode_normalize(2) == 2);
	assert(ftp_share_mode_normalize(3) == 3);
	assert(ftp_share_mode_normalize(4) == 4);

	assert(ftp_share_mode_normalize(0) == 2);
	assert(ftp_share_mode_normalize(-1) == 2);
	assert(ftp_share_mode_normalize(5) == 2);
	assert(ftp_share_mode_normalize(INT_MIN) == 2);
	assert(ftp_share_mode_normalize(INT_MAX) == 2);

	puts("FTP share-mode fail-closed normalization: PASS");
	return 0;
}
