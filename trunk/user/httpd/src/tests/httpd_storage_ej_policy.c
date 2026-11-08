#include <assert.h>
#include <stddef.h>

#include "httpd_storage_ej_policy.h"

int
main(void)
{
	static const char * const mutating_callbacks[] = {
		"set_AiDisk_status",
		"safely_remove_disk",
		"set_account_permission",
		"initial_account",
		"create_account",
		"delete_account",
		"modify_account",
		"create_sharedfolder",
		"delete_sharedfolder",
		"modify_sharedfolder",
		"set_share_mode",
		NULL
	};
	static const char * const read_only_callbacks[] = {
		"get_usb_share_list",
		"get_AiDisk_status",
		"get_all_accounts",
		"get_permissions_of_account",
		"get_folder_tree",
		"get_share_tree",
		NULL
	};
	int i;

	assert(!httpd_storage_ej_allowed(NULL, 0));
	assert(!httpd_storage_ej_allowed(NULL, 1));
	assert(!httpd_storage_ej_allowed("", 0));
	assert(!httpd_storage_ej_allowed("", 1));

	for (i = 0; mutating_callbacks[i] != NULL; ++i) {
		assert(httpd_storage_ej_requires_trusted_session(mutating_callbacks[i]));
		assert(!httpd_storage_ej_allowed(mutating_callbacks[i], 0));
		assert(httpd_storage_ej_allowed(mutating_callbacks[i], 1));
	}

	for (i = 0; read_only_callbacks[i] != NULL; ++i) {
		assert(!httpd_storage_ej_requires_trusted_session(read_only_callbacks[i]));
		assert(httpd_storage_ej_allowed(read_only_callbacks[i], 0));
		assert(httpd_storage_ej_allowed(read_only_callbacks[i], 1));
	}

	/* Exact names only; preserve unrelated handlers' historical behavior. */
	assert(!httpd_storage_ej_requires_trusted_session("create_account_extra"));
	assert(httpd_storage_ej_allowed("create_account_extra", 0));
	assert(httpd_storage_ej_allowed("system_status_data", 0));

	return 0;
}
